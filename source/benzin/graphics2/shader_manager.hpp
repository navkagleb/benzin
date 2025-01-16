#pragma once

#include "benzin/graphics/dxc_shader_compiler.hpp"
#include "benzin/graphics/shader.hpp"

namespace benzin
{

    class Win64ShaderFileWatcher
    {
    public:
        using Callback = std::function<void(std::filesystem::path&& filePath)>;

        Win64ShaderFileWatcher();
        ~Win64ShaderFileWatcher();

        void SetCallback(Callback&& callback) { m_Callback = std::move(callback); }

    private:
        using RawFileInfoBuffer = std::array<std::byte, 1_kb>;

        void WatchFiles();
        void HandleFileChanges(const RawFileInfoBuffer& rawFileInfoBuffer);

    private:
        const std::filesystem::path& m_WatchDirectory;

        HANDLE m_DirectoryHandle = INVALID_HANDLE_VALUE;
        HANDLE m_StoppedEvent = INVALID_HANDLE_VALUE;
        OVERLAPPED m_DirectoryChangeOverlapped{};

        std::thread m_WatchThread;

        Callback m_Callback;
    };

    class ShaderManager
    {
    public:
        using NewShaderAvailableCallback = std::function<void()>;

        ShaderManager();
        ~ShaderManager();

        bool IsEachShaderGood() const { return m_IsEachShaderGood; }

        void SetNewShaderAvailableCallback(NewShaderAvailableCallback&& callback) { m_NewShaderAvailableCallback = std::move(callback); }

    public:
        ShaderBytecode GetShaderBytecode(const ShaderInfo& shader);

        void CheckForNewShader();
        bool CompareWithNewShader(const ShaderInfo& shader);

    private:
        void CacheIncludeDependencies();
        void LoadIncludeDependenciesCache();

        bool TryCompileShader(const ShaderInfo& shader);
        bool LoadShader(const ShaderInfo& shader);

        bool IsNewShaderAvailable() const;
        void FileWatcherCallback(std::filesystem::path&& filePath);

    private:
        const DxcShaderCompiler m_ShaderCompiler;
        Win64ShaderFileWatcher m_FileWatcher;

        // TODO: Maybe replace with one big unordered_map?
        std::unordered_map<uint64_t, std::vector<std::byte>> m_ShaderDxils;
        std::unordered_map<uint64_t, std::unordered_set<std::filesystem::path>> m_IncludeDependencies;

        std::mutex m_NewShaderMutex;
        std::optional<std::filesystem::path> m_NewShader;

        NewShaderAvailableCallback m_NewShaderAvailableCallback;

        bool m_IsEachShaderGood = true;
    };

}
