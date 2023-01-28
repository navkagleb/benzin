#pragma once

#include "benzin/graphics/common.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    class Window;

    enum class VSyncState : bool
    {
        Enabled = true,
        Disabled = false
    };

    class SwapChain
    {
    public:
        BENZIN_NON_COPYABLE(SwapChain)
        BENZIN_NON_MOVEABLE(SwapChain)
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_DXGISwapChain3, "SwapChain")

    public:
        SwapChain(const Window& window, Device& device, CommandQueue& commandQueue);
        ~SwapChain();

    public:
        GraphicsFormat GetBackBufferFormat() const { return m_BackBufferFormat; }

    public:
        std::shared_ptr<TextureResource>& GetCurrentBackBuffer();
        const std::shared_ptr<TextureResource>& GetCurrentBackBuffer() const;

    private:
        uint32_t GetCurrentBufferIndex() const;

    public:
        void ResizeBackBuffers(Device& device, uint32_t width, uint32_t height);
        void Flip(VSyncState vsync);

    private:
        void EnumerateAdapters();

        void CreateBackBuffers(Device& device);

    private:
        IDXGIFactory4* m_DXGIFactory4{ nullptr };
        IDXGISwapChain3* m_DXGISwapChain3{ nullptr };

        GraphicsFormat m_BackBufferFormat{ GraphicsFormat::R8G8B8A8UnsignedNorm };
        std::vector<std::shared_ptr<TextureResource>> m_BackBuffers;
    };

} // namespace benzin
