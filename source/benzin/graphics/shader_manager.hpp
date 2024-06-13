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

        std::span<const std::byte> GetShaderDxil(const ShaderCreation& shaderCreation);
        std::span<const std::byte> GetLibraryDxil(std::string_view fileName);

        void RunIfPendingToReloadShaderAvailable(std::function<void()>&& callback);
        bool UpdateShaderState(const ShaderCreation& shaderCreation);

    private:
        bool IsPendingToReloadShaderAvailable() const;

        void FileWatcherCallback(std::filesystem::path&& filePath);

    private:
        const DxcShaderCompiler m_DxcShaderCompiler;
        const Win64_ShaderFileWatcher m_FileWatcher;

        std::unordered_map<uint64_t, std::vector<std::byte>> m_ShaderDxils;
        std::unordered_map<std::filesystem::path, std::unordered_set<std::filesystem::path>> m_IncludeDependencies;

        std::mutex m_PendingShaderToReloadMutex;
        std::optional<std::filesystem::path> m_PendingShaderToReload;
    };

}
