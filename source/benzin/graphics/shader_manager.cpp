#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/shader_manager.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/pipeline_state.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    static const auto g_IncludeDependenciesFilePath = std::filesystem::absolute("bin/shader_include_dependencies.txt");

    static size_t GetShaderHash(const ShaderCreation& shaderCreation)
    {
        size_t hash = 0;
        hash = HashCombine(hash, magic_enum::enum_integer(shaderCreation.Type));
        hash = HashCombine(hash, shaderCreation.FileName);
        hash = HashCombine(hash, shaderCreation.EntryPoint);

        return hash;
    }

    static bool IsIncludeShader(std::wstring_view fileName)
    {
        return fileName.ends_with(L"hlsli");
    }

    static bool IsSourceShader(std::wstring_view fileName)
    {
        return fileName.ends_with(L"hlsl");
    }

    static void CacheShader(const ShaderPaths& paths, const CompiledShader& compiledShader)
    {
        WriteToFile(paths.DxilFilePath, compiledShader.DxilBlob);

        if constexpr (config::g_IsShaderSymbolsEnabled)
        {
            BenzinAssert(!compiledShader.PdbBlob.empty());
            WriteToFile(paths.PdbFilePath, compiledShader.PdbBlob);
        }
    }

    // Win64_ShaderFileWatcher

    Win64_ShaderFileWatcher::Win64_ShaderFileWatcher(Callback&& callback)
        : m_WatchDirectory{ Dxc_ShaderCompiler::GetShaderSourceDir() }
        , m_Callback{ callback }
    {
        BenzinAssert((bool)callback);

        m_DirectoryHandle = ::CreateFileW(
            m_WatchDirectory.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );
        BenzinEnsure(m_DirectoryHandle != INVALID_HANDLE_VALUE);

        m_StoppedEvent = ::CreateEventW(nullptr, true, false, nullptr);
        BenzinEnsure(m_StoppedEvent != INVALID_HANDLE_VALUE);

        m_DirectoryChangeOverlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        BenzinEnsure(m_DirectoryChangeOverlapped.hEvent != INVALID_HANDLE_VALUE);

        m_WatchThread = std::thread(&Win64_ShaderFileWatcher::WatchFiles, this);
    }

    Win64_ShaderFileWatcher::~Win64_ShaderFileWatcher()
    {
        ::SetEvent(m_StoppedEvent);

        if (m_WatchThread.joinable())
        {
            m_WatchThread.join();
        }

        ::CloseHandle(m_DirectoryHandle);
        ::CloseHandle(m_StoppedEvent);
        ::CloseHandle(m_DirectoryChangeOverlapped.hEvent);
    }

    void Win64_ShaderFileWatcher::WatchFiles()
    {
        const auto handles = std::to_array({ m_StoppedEvent, m_DirectoryChangeOverlapped.hEvent });

        std::array<std::byte, 1024> buffer;

        while (true)
        {
            const bool isSucceeded = ::ReadDirectoryChangesW(
                m_DirectoryHandle,
                buffer.data(),
                (DWORD)buffer.size(),
                true, // is recursive
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, // notify filter
                nullptr,
                &m_DirectoryChangeOverlapped,
                nullptr
            ) != 0;

            if (!isSucceeded)
            {
                continue;
            }

            const DWORD waitStatus = ::WaitForMultipleObjects(
                (DWORD)handles.size(),
                handles.data(),
                false, // Wait for all
                INFINITE
            );

            if (waitStatus == WAIT_OBJECT_0)
            {
                break;
            }

            BenzinAssert(waitStatus == WAIT_OBJECT_0 + 1);
            ::ResetEvent(m_DirectoryChangeOverlapped.hEvent);

            // Used only for ensuring that everything is ok
            DWORD writeByteCount;
            BenzinEnsure(::GetOverlappedResult(m_DirectoryHandle, &m_DirectoryChangeOverlapped, &writeByteCount, false) != 0);

            const FILE_NOTIFY_INFORMATION* fileInfo = nullptr;
            size_t bufferOffset = 0;

            do
            {
                fileInfo = (FILE_NOTIFY_INFORMATION*)(buffer.data() + bufferOffset);
                bufferOffset += fileInfo->NextEntryOffset;

                const std::wstring_view fileName{ fileInfo->FileName, fileInfo->FileNameLength / sizeof(WCHAR) };
                std::filesystem::path filePath = m_WatchDirectory / fileName;

                if (!IsIncludeShader(fileName) && !IsSourceShader(fileName))
                {
                    continue;
                }

                if (
                    fileInfo->Action == FILE_ACTION_MODIFIED ||
                    // Case for Visual Studio
                    // Visual Studio creates temp file and then rename it to original file name
                    fileInfo->Action == FILE_ACTION_RENAMED_NEW_NAME
                )
                {
                    m_Callback(std::move(filePath));
                }
            } while (fileInfo->NextEntryOffset != 0);
        }
    }

    // ShaderManager

    ShaderManager::ShaderManager()
        : m_FileWatcher{ std::bind(&ShaderManager::FileWatcherCallback, this, std::placeholders::_1) }
    {
        LoadIncludeDependenciesCache();
    }

    ShaderManager::~ShaderManager()
    {
        CacheIncludeDependencies();
    }

    std::span<const std::byte> ShaderManager::GetShaderDxil(const ShaderCreation& shaderCreation, bool isCacheIgnored)
    {
        const size_t shaderHash = GetShaderHash(shaderCreation);
        
        const auto it = m_ShaderDxils.find(shaderHash);
        if (it != m_ShaderDxils.end())
        {
            return it->second;
        }

        if (!isCacheIgnored && LoadShaderCacheIfPossible(shaderCreation))
        {
            return m_ShaderDxils.at(shaderHash);
        }

        BenzinEnsure(TryCompileShaderIfNeeded(shaderCreation));
        return m_ShaderDxils.at(shaderHash);
    }

    std::span<const std::byte> ShaderManager::GetLibraryDxil(std::string_view fileName)
    {
        return GetShaderDxil(ShaderCreation
        {
            .Type = ShaderType::Library,
            .FileName = fileName,
        });
    }

    bool ShaderManager::TryCompileShaderIfNeeded(const ShaderCreation& shaderCreation)
    {
        const size_t shaderHash = GetShaderHash(shaderCreation);

        auto& isShaderGood = m_IsShaderGoodMap[shaderHash];
        if (isShaderGood)
        {
            return true;
        }

        const ShaderPaths paths{ shaderHash, shaderCreation.FileName };
        const ShaderArgs args{ shaderCreation.Type, shaderCreation.EntryPoint };
        auto [us, compiledShader] = BenzinProfileFunction(m_ShaderCompiler.CompileShader(paths, args));

        if (!compiledShader.IsValid())
        {
            isShaderGood = false;
            return false;
        }

        if (shaderCreation.Type == ShaderType::Library)
        {
            BenzinTrace("LibraryCompiled: {}! File: {}, Time: {} ms", shaderHash, shaderCreation.FileName, ToFloatMs(us));
        }
        else
        {
            BenzinTrace(
                "ShaderCompiled: {}! File: {}, EntryPoint: {}. Time: {} ms",
                shaderHash,
                shaderCreation.FileName,
                shaderCreation.EntryPoint,
                ToFloatMs(us)
            );
        }

        CacheShader(paths, compiledShader);

        isShaderGood = true;
        m_ShaderDxils[shaderHash] = std::move(compiledShader.DxilBlob);
        m_IncludeDependencies[shaderHash] = std::move(compiledShader.IncludeFilePaths);

        return true;
    }

    bool ShaderManager::UpdateShaderState(const ShaderCreation& shaderCreation)
    {
        BenzinAssert(IsPendingToReloadShaderAvailable());

        const size_t shaderHash = GetShaderHash(shaderCreation);
        const ShaderPaths paths{ shaderHash, shaderCreation.FileName };

        bool isShaderNeedsRecompilation = true;

        BenzinExecuteOnScopeExit([&]
        {
            if (isShaderNeedsRecompilation)
            {   
                m_IsShaderGoodMap[shaderHash] = false;
                m_ShaderDxils.erase(shaderHash);
            }
        });

        if (IsSourceShader(m_PendingShaderToReload->c_str()))
        {
            isShaderNeedsRecompilation = *m_PendingShaderToReload == paths.SourceFilePath;
            return isShaderNeedsRecompilation;
        }

        BenzinAssert(m_IncludeDependencies.contains(shaderHash));
        if (m_IncludeDependencies.at(shaderHash).contains(*m_PendingShaderToReload))
        {
            return isShaderNeedsRecompilation;
        }

        return isShaderNeedsRecompilation = false;
    }

    void ShaderManager::RunIfPendingToReloadShaderIsAvailable(std::function<void()>&& callback)
    {
        {
            std::lock_guard guard{ m_PendingShaderToReloadMutex };

            if (!IsPendingToReloadShaderAvailable())
            {
                return;
            }

            callback();

            m_PendingShaderToReload = std::nullopt;
        }

        m_IsAllShaderGood = std::ranges::all_of(m_IsShaderGoodMap, [](const auto& pair)
        {
            return pair.second;
        });
    }

    bool ShaderManager::IsPendingToReloadShaderAvailable() const
    {
        if (!m_PendingShaderToReload.has_value())
        {
            return false;
        }

        // Checks if the file is in use by another process
        // If so, the shader compilation will fail

        const HANDLE fileHandle = ::CreateFileW(
            m_PendingShaderToReload->c_str(),
            GENERIC_READ, // open for reading
            0, // do not share
            nullptr, // default security
            OPEN_EXISTING, // existing file only
            FILE_ATTRIBUTE_NORMAL, // normal file
            nullptr // no attribute template
        );

        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        ::CloseHandle(fileHandle);
        return true;
    }

    void ShaderManager::CacheIncludeDependencies()
    {
        BenzinAssert(!m_IncludeDependencies.empty());

        std::ofstream file{ g_IncludeDependenciesFilePath };

        for (const auto& [shaderHash, includePaths] : m_IncludeDependencies)
        {
            file << shaderHash << '\n';

            for (const auto& includePath : includePaths)
            {
                file << includePath.string() << '\n';
            }

            file << '\n';
        }
    }

    void ShaderManager::LoadIncludeDependenciesCache()
    {
        if (!std::filesystem::exists(g_IncludeDependenciesFilePath))
        {
            return;
        }

        BenzinAssert(m_IncludeDependencies.empty());

        std::ifstream file{ g_IncludeDependenciesFilePath };

        std::string line;
        while (std::getline(file, line))
        {
            const auto shaderHash = ToU64(line);

            std::unordered_set<std::filesystem::path> includeDependencies;
            while (std::getline(file, line) && !line.empty())
            {
                includeDependencies.insert(line);
            }

            m_IncludeDependencies[shaderHash] = std::move(includeDependencies);
        }
    }

    bool ShaderManager::LoadShaderCacheIfPossible(const ShaderCreation& shaderCreation)
    {
        if (CommandLineArgs::g_IsShaderCacheIgnored)
        {
            return false;
        }

        const size_t shaderHash = GetShaderHash(shaderCreation);
        const ShaderPaths paths{ shaderHash, shaderCreation.FileName };

        if (IsDestinationFileOlder(paths.SourceFilePath, paths.DxilFilePath))
        {
            return false;
        }

        const bool isAnyIncludeDependencyNewer = std::ranges::any_of(
            m_IncludeDependencies.at(shaderHash),
            [&paths](const std::filesystem::path& includeDependency)
            {
                return IsDestinationFileOlder(includeDependency, paths.DxilFilePath);
            }
        );

        if (isAnyIncludeDependencyNewer)
        {
            return false;
        }

        auto [us, shaderDxil] = BenzinProfileFunction(ReadFromFile(paths.DxilFilePath));

        m_IsShaderGoodMap[shaderHash] = true;
        m_ShaderDxils[shaderHash] = std::move(shaderDxil);

        BenzinTrace(
            "Shader loaded from cache: {}! File: {}, EntryPoint: {}. Time: {} ms",
            shaderHash,
            shaderCreation.FileName,
            shaderCreation.EntryPoint,
            ToFloatMs(us)
        );

        return true;
    }

    void ShaderManager::FileWatcherCallback(std::filesystem::path&& filePath)
    {
        const std::lock_guard lock{ m_PendingShaderToReloadMutex };

        BenzinAssert(!m_PendingShaderToReload.has_value() || m_PendingShaderToReload == filePath);
        m_PendingShaderToReload = std::move(filePath);
    }

}
