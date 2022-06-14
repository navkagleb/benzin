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
        uint32_t GetCurrentBufferIndex() const;

        Texture2D& GetCurrentBuffer();
        const Texture2D& GetCurrentBuffer() const;

    public:
        bool ResizeBuffers(Device& device, Context& context, uint32_t width, uint32_t height);
        bool Present(VSyncState vsync);

    private:
        bool Init(Device& device, Context& context, Window& window, const SwapChainConfig& config);

        bool InitFactory();
        bool InitSwapChain(Context& context, Window& window);

        bool CreateBuffers(Device& device, uint32_t width, uint32_t height);

    private:
        ComPtr<IDXGIFactory> m_Factory;
        ComPtr<IDXGISwapChain> m_SwapChain;
        ComPtr<IDXGISwapChain3> m_SwapChain3;

        GraphicsFormat m_BufferFormat;
        std::vector<Texture2D> m_Buffers;
    };

} // namespace spieler::renderer