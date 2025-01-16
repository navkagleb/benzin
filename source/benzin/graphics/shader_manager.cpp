#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/shader_manager.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    static const auto g_IncludeDependenciesFilePath = std::filesystem::absolute("bin/shader_include_dependencies.txt");

    static void LogShaderInfo(std::string_view stage, std::chrono::microseconds stageTime, const ShaderInfo& shader)
    {
        BenzinTrace(
            "{} ({:07.3f} ms): {:20}! Type: {:>7}, File: {}, EntryPoint: {}, Defines: {}",
            stage,
            ToFloatMs(stageTime),
            shader.GetHash(),
            magic_enum::enum_name(shader.GetType()),
            shader.GetFileName(),
            !shader.GetEntryPoint().empty() ? shader.GetEntryPoint() : "\"-\"",
            shader.GetDefines()
        );
    }

    static bool IsIncludeShader(std::wstring_view fileName)
    {
        return fileName.ends_with(L"hlsli");
    }

    static bool IsSourceShader(std::wstring_view fileName)
    {
        return fileName.ends_with(L"hlsl") || fileName.ends_with(L"hpp");
    }

    static void CacheShader(const ShaderPaths& paths, const CompiledShader& compiledShader)
    {
        WriteToFile(paths.DxilFilePath, compiledShader.DxilBlob);

        if (GfxConfig::s_IsShaderSymbolsEnabled)
        {
            BenzinAssert(!compiledShader.PdbBlob.empty());
            WriteToFile(paths.PdbFilePath, compiledShader.PdbBlob);
        }
    }

    // Win64ShaderFileWatcher

    Win64ShaderFileWatcher::Win64ShaderFileWatcher()
        : m_WatchDirectory{ GfxConfig::s_ShaderSourceDir }
    {
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

        m_WatchThread = std::thread(&Win64ShaderFileWatcher::WatchFiles, this);
    }

    Win64ShaderFileWatcher::~Win64ShaderFileWatcher()
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

    void Win64ShaderFileWatcher::WatchFiles()
    {
        const auto handles = std::to_array({ m_StoppedEvent, m_DirectoryChangeOverlapped.hEvent });

        RawFileInfoBuffer rawFileInfoBuffer{};

        while (true)
        {
            const bool isSucceeded = ::ReadDirectoryChangesW(
                m_DirectoryHandle,
                rawFileInfoBuffer.data(),
                (DWORD)rawFileInfoBuffer.size(),
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

            HandleFileChanges(rawFileInfoBuffer);
        }
    }

    void Win64ShaderFileWatcher::HandleFileChanges(const RawFileInfoBuffer& rawFileInfoBuffer)
    {
        size_t bufferOffset = 0;

        while (true)
        {
            const auto* fileInfo = (FILE_NOTIFY_INFORMATION*)(rawFileInfoBuffer.data() + bufferOffset);
            bufferOffset += fileInfo->NextEntryOffset;

            const std::wstring_view fileName{ fileInfo->FileName, fileInfo->FileNameLength / sizeof(WCHAR) };
            std::filesystem::path filePath = m_WatchDirectory / fileName;

            if (IsIncludeShader(fileName) || IsSourceShader(fileName))
            {
                const bool isFileChanged =
                    fileInfo->Action == FILE_ACTION_MODIFIED ||
                    // Case for Visual Studio
                    // Visual Studio creates temp file and then rename it to original file name
                    fileInfo->Action == FILE_ACTION_RENAMED_NEW_NAME;

                if (isFileChanged && m_Callback)
                {
                    m_Callback(std::move(filePath));
                }
            }

            if (fileInfo->NextEntryOffset == 0)
            {
                break;
            }
        }
    }

    // ShaderManager

    ShaderManager::ShaderManager()
    {
        LoadIncludeDependenciesCache();

        m_FileWatcher.SetCallback([this](std::filesystem::path&& filePath) { return FileWatcherCallback(std::move(filePath)); });
    }

    ShaderManager::~ShaderManager()
    {
        CacheIncludeDependencies();
    }

    ShaderBytecode ShaderManager::GetShaderBytecode(const ShaderInfo& shader)
    {
        if (!m_ShaderDxils.contains(shader.GetHash()))
        {
            if (!LoadShader(shader) && !TryCompileShader(shader))
            {
                m_IsEachShaderGood = false;
                return {};
            }
        }

        return m_ShaderDxils.at(shader.GetHash());
    }

    void ShaderManager::CheckForNewShader()
    {
        std::lock_guard guard{ m_NewShaderMutex };

        if (IsNewShaderAvailable() && m_NewShaderAvailableCallback)
        {
            m_IsEachShaderGood = true;

            m_NewShaderAvailableCallback();
            m_NewShader = std::nullopt;
        }
    }

    bool ShaderManager::CompareWithNewShader(const ShaderInfo& shader)
    {
        BenzinAssert(IsNewShaderAvailable());
        BenzinAssert(shader.IsValid());

        const ShaderPaths paths{ shader };

        bool isShaderNeedsRecompilation = false;
        if (IsSourceShader(m_NewShader->c_str()))
        {
            isShaderNeedsRecompilation = *m_NewShader == paths.SourceFilePath;
        }
        else if (IsIncludeShader(m_NewShader->c_str()))
        {
            BenzinAssert(m_IncludeDependencies.contains(shader.GetHash()));
            isShaderNeedsRecompilation = m_IncludeDependencies.at(shader.GetHash()).contains(*m_NewShader);
        }

        if (isShaderNeedsRecompilation)
        {
            m_ShaderDxils.erase(shader.GetHash());
        }

        return isShaderNeedsRecompilation;
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

    bool ShaderManager::TryCompileShader(const ShaderInfo& shader)
    {
        if (m_ShaderDxils.contains(shader.GetHash()))
        {
            return true;
        }

        const ShaderPaths paths{ shader };
        const ShaderArgs args{ shader };
        auto [us, compiledShader] = BenzinProfileFunction(m_ShaderCompiler.CompileShader(paths, args));

        if (!compiledShader.IsValid())
        {
            return false;
        }

        LogShaderInfo("Shader compiled", us, shader);
        CacheShader(paths, compiledShader);

        m_ShaderDxils[shader.GetHash()] = std::move(compiledShader.DxilBlob);
        m_IncludeDependencies[shader.GetHash()] = std::move(compiledShader.IncludeFilePaths);

        return true;
    }

    bool ShaderManager::LoadShader(const ShaderInfo& shader)
    {
        if (CommandLineArgs::GetBool("IsShaderCacheIgnored"))
        {
            return false;
        }

        const ShaderPaths paths{ shader };

        if (IsDestinationFileOlder(paths.SourceFilePath, paths.DxilFilePath))
        {
            return false;
        }

        if (!m_IncludeDependencies.contains(shader.GetHash()))
        {
            // It is impossible to check the validity of a shader
            // relative to its included dependencies. So it's better to compile it
            return false;
        }

        const bool isAnyIncludeDependencyNewer = std::ranges::any_of(
            m_IncludeDependencies.at(shader.GetHash()),
            [&paths](const std::filesystem::path& includeDependency)
            {
                return !std::filesystem::exists(includeDependency) || IsDestinationFileOlder(includeDependency, paths.DxilFilePath);
            }
        );

        if (isAnyIncludeDependencyNewer)
        {
            return false;
        }

        auto [us, shaderDxil] = BenzinProfileFunction(ReadFromFile(paths.DxilFilePath));
        m_ShaderDxils[shader.GetHash()] = std::move(shaderDxil);

        LogShaderInfo("Shader loaded  ", us, shader);

        return true;
    }

    bool ShaderManager::IsNewShaderAvailable() const
    {
        if (!m_NewShader.has_value())
        {
            return false;
        }

        // Checks if the file is in use by another process
        // If so, the shader compilation will fail

        const HANDLE fileHandle = ::CreateFileW(
            m_NewShader->c_str(),
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

    void ShaderManager::FileWatcherCallback(std::filesystem::path&& filePath)
    {
        const std::lock_guard lock{ m_NewShaderMutex };

        BenzinAssert(!m_NewShader.has_value() || m_NewShader == filePath);
        m_NewShader = std::move(filePath);

        BenzinTrace("Shader '{}' updated", m_NewShader->string());
    }

}
