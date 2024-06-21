#pragma once

#include "benzin/graphics/dxc_shader_compiler.hpp"

namespace benzin
{

    struct ShaderCreation;

    class ShaderManager;

    class Win64_ShaderFileWatcher
    {
    public:
        using Callback = std::function<void(std::filesystem::path&& filePath)>;

        Win64_ShaderFileWatcher(Callback&& callback);
        ~Win64_ShaderFileWatcher();

    private:
        void WatchFiles();

    private:
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

        std::span<const std::byte> GetShaderDxil(const ShaderCreation& shaderCreation, bool isCacheIgnored = false);
        std::span<const std::byte> GetLibraryDxil(std::string_view fileName);

        bool TryCompileShaderIfNeeded(const ShaderCreation& shaderCreation);

        void RunIfPendingToReloadShaderIsAvailable(std::function<void()>&& callback);
        bool UpdateShaderState(const ShaderCreation& shaderCreation);

    private:
        bool IsPendingToReloadShaderAvailable() const;

        void CacheIncludeDependencies();
        void LoadIncludeDependenciesCache();

        bool LoadShaderCacheIfPossible(const ShaderCreation& shaderCreation);

        void FileWatcherCallback(std::filesystem::path&& filePath);

    private:
        const DxcShaderCompiler m_ShaderCompiler;
        const Win64_ShaderFileWatcher m_FileWatcher;

        // TODO: Maybe replace with one big unordered_map?
        std::unordered_map<uint64_t, bool> m_IsShaderGoodMap;
        std::unordered_map<uint64_t, std::vector<std::byte>> m_ShaderDxils;
        std::unordered_map<uint64_t, std::unordered_set<std::filesystem::path>> m_IncludeDependencies;

        std::mutex m_PendingShaderToReloadMutex;
        std::optional<std::filesystem::path> m_PendingShaderToReload;

        bool m_IsAllShaderGood = true;
    };

}
