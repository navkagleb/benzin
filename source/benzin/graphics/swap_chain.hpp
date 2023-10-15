#pragma once

#include "benzin/graphics/fence.hpp"

namespace benzin
{

    class Window;

    class Backend;
    class Buffer;
    class Device;
    class Texture;

    class SwapChain
    {
    public:
        BENZIN_NON_COPYABLE_IMPL(SwapChain)
        BENZIN_NON_MOVEABLE_IMPL(SwapChain)

    public:
        SwapChain(const Window& window, const Backend& backend, Device& device);
        ~SwapChain();

    public:
        IDXGISwapChain3* GetDXGISwapChain() const { return m_DXGISwapChain; }

        uint32_t GetCurrentBackBufferIndex() const { return m_DXGISwapChain->GetCurrentBackBufferIndex(); }

        std::shared_ptr<Texture>& GetCurrentBackBuffer() { return m_BackBuffers[GetCurrentBackBufferIndex()]; }
        const std::shared_ptr<Texture>& GetCurrentBackBuffer() const { return m_BackBuffers[GetCurrentBackBufferIndex()]; }

        uint64_t GetCPUFrameIndex() const { return m_CPUFrameIndex; }
        uint64_t GetGPUFrameIndex() const { return m_GPUFrameIndex; }

        bool IsVSyncEnabled() const { return m_IsVSyncEnabled; }
        void SetVSyncEnabled(bool isEnabled) { m_IsVSyncEnabled = isEnabled; }

        float GetAspectRatio() const { return m_AspectRatio; }
        const Viewport& GetViewport() const { return m_Viewport; }
        const ScissorRect& GetScissorRect() const { return m_ScissorRect; }

    public:
        void ResizeBackBuffers(uint32_t width, uint32_t height);
        void Flip();

    private:
        void RegisterBackBuffers();
        void UpdateViewportDimensions(float width, float height);

        void FlushAndResetBackBuffers();
        void WaitForGPU();

    private:
        Device& m_Device;

        IDXGISwapChain3* m_DXGISwapChain = nullptr;

        std::array<std::shared_ptr<Texture>, config::g_BackBufferCount> m_BackBuffers;

        Fence m_FrameFence;
        uint64_t m_CPUFrameIndex = 0;
        uint64_t m_GPUFrameIndex = 0;

        uint32_t m_DXGISwapChainFlags = 0;
        uint32_t m_DXGIPresentFlags = 0;
        bool m_IsVSyncEnabled = false;

        float m_AspectRatio = 0.0f;
        Viewport m_Viewport;
        ScissorRect m_ScissorRect;
    };

} // namespace benzin
