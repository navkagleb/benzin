#pragma once

#include "benzin/graphics/api/texture.hpp"
#include "benzin/graphics/api/fence.hpp"

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
        SwapChain(const Window& window, Device& device, CommandQueue& commandQueue, std::string_view debugName);
        ~SwapChain();

    public:
        uint32_t GetCurrentBackBufferIndex() const { return m_DXGISwapChain3->GetCurrentBackBufferIndex(); }

        std::shared_ptr<TextureResource>& GetCurrentBackBuffer() { return m_BackBuffers[GetCurrentBackBufferIndex()]; }
        const std::shared_ptr<TextureResource>& GetCurrentBackBuffer() const { return m_BackBuffers[GetCurrentBackBufferIndex()]; }

    public:
        void ResizeBackBuffers(Device& device, uint32_t width, uint32_t height);
        void Flip(VSyncState vsync);

    private:
        void EnumerateAdapters();

        void RegisterBackBuffers(Device& device);

    private:
        CommandQueue& m_CommandQueue;

        IDXGIFactory4* m_DXGIFactory4{ nullptr };
        IDXGISwapChain3* m_DXGISwapChain3{ nullptr };

        std::array<std::shared_ptr<TextureResource>, config::GetBackBufferCount()> m_BackBuffers;

        Fence m_FrameFence;
        uint64_t m_CPUFrameIndex{ 0 };
        uint64_t m_GPUFrameIndex{ 0 };
    };

} // namespace benzin
