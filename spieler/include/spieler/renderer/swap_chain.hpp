#pragma once

#include "spieler/system/window.hpp"

#include "spieler/renderer/texture.hpp"
#include "spieler/renderer/resource_view.hpp"
#include "spieler/renderer/types.hpp"

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
        GraphicsFormat BufferFormat{ GraphicsFormat::UNKNOWN };
        GraphicsFormat DepthStencilFormat{ GraphicsFormat::UNKNOWN };
    };

    class SwapChain
    {
    public:
        SwapChain(Device& device, Context& context, Window& window, const SwapChainConfig& config);
        ~SwapChain();

    public:
        GraphicsFormat GetBufferFormat() const { return m_BufferFormat; }
        GraphicsFormat GetDepthStencilFormat() const { return m_DepthStencilFormat; }

    public:
        Texture2D& GetCurrentBuffer();
        const Texture2D& GetCurrentBuffer() const;
        const Texture2D& GetDepthStencil() const { return m_DepthStencil; }

        bool Init(Device& device, Context& context, Window& window);

        bool ResizeBuffers(Device& device, Context& context, uint32_t width, uint32_t height);

        bool Present(VSyncState vsync);

    private:
        bool InitFactory();
        bool InitSwapChain(Context& context, Window& window);

        bool CreateBuffers(Device& device, uint32_t width, uint32_t height);
        bool CreateDepthStencil(Device& device, uint32_t width, uint32_t height);

    private:
        ComPtr<IDXGIFactory> m_Factory;
        ComPtr<IDXGISwapChain> m_SwapChain;
        ComPtr<IDXGISwapChain3> m_SwapChain3;

        uint32_t m_BufferCount{ 0 };

        // Back buffers
        GraphicsFormat m_BufferFormat;
        std::vector<Texture2D> m_Buffers;

        // DepthStencil
        GraphicsFormat m_DepthStencilFormat;
        Texture2D m_DepthStencil;
    };

} // namespace spieler::renderer