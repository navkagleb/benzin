#pragma once

#include "spieler/system/window.hpp"

#include "spieler/renderer/texture.hpp"
#include "spieler/renderer/resource_view.hpp"
#include "spieler/renderer/common.hpp"

namespace spieler::renderer
{

    class Device;
    class Context;

    enum class VSyncState : bool
    {
        Enabled = true,
        Disabled = false
    };

    struct SwapChainConfig
    {
        uint32_t BufferCount{ 0 };
        GraphicsFormat BufferFormat{ GraphicsFormat::Unknown };
    };

    class SwapChain
    {
    public:
        SwapChain(Device& device, Context& context, Window& window, const SwapChainConfig& config);
        ~SwapChain();

    public:
        GraphicsFormat GetBufferFormat() const { return m_BufferFormat; }

    public:
        Texture& GetCurrentBuffer();
        const Texture& GetCurrentBuffer() const;

    private:
        uint32_t GetCurrentBufferIndex() const;

    public:
        void ResizeBuffers(Device& device, uint32_t width, uint32_t height);
        void Present(VSyncState vsync);

    private:
        bool Init(Device& device, Context& context, Window& window, const SwapChainConfig& config);

        bool InitFactory();
        bool InitSwapChain(Context& context, Window& window);

        void EnumerateAdapters();

        void CreateBuffers(Device& device);

    private:
        ComPtr<IDXGIFactory> m_DXGIFactory;
        ComPtr<IDXGISwapChain> m_DXGISwapChain;
        ComPtr<IDXGISwapChain3> m_DXGISwapChain3;
        std::vector<ComPtr<IDXGIAdapter>> m_Adapters;

        GraphicsFormat m_BufferFormat;
        std::vector<Texture> m_Buffers;
    };

} // namespace spieler::renderer
