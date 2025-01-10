#pragma once

#include "benzin/graphics/dxc_shader_compiler.hpp"

namespace benzin
{

    class ShaderInfo;

    class Win64ShaderFileWatcher
    {
    public:
        using Callback = std::function<void(std::filesystem::path&& filePath)>;

        Win64ShaderFileWatcher(Callback&& callback);
        ~Win64ShaderFileWatcher();

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
        ShaderManager();
        ~ShaderManager();

        bool IsAllShadersGood() const { return m_IsAllShaderGood; }

        std::span<const std::byte> GetShaderDxil(const ShaderInfo& shader, bool isCacheIgnored = false);

        bool TryCompileShaderIfNeeded(const ShaderInfo& shader);

        void RunIfPendingToReloadShaderIsAvailable(std::function<void()>&& callback);
        bool UpdateShaderState(const ShaderInfo& shader);

    private:
        bool IsPendingToReloadShaderAvailable() const;

        void CacheIncludeDependencies();
        void LoadIncludeDependenciesCache();

        bool LoadShaderCacheIfPossible(const ShaderInfo& shader);

        void FileWatcherCallback(std::filesystem::path&& filePath);

    private:
        const DxcShaderCompiler m_ShaderCompiler;
        const Win64ShaderFileWatcher m_FileWatcher;

        // TODO: Maybe replace with one big unordered_map?
        std::unordered_map<uint64_t, bool> m_IsShaderGoodMap;
        std::unordered_map<uint64_t, std::vector<std::byte>> m_ShaderDxils;
        std::unordered_map<uint64_t, std::unordered_set<std::filesystem::path>> m_IncludeDependencies;

        std::mutex m_PendingShaderToReloadMutex;
        std::optional<std::filesystem::path> m_PendingShaderToReload;

        bool m_IsAllShaderGood = true;
    };

}
