#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Window;

    class Backend;
    class Buffer;
    class Device;
    class Fence;
    class Texture;

    class FrameFence
    {
    public:
        explicit FrameFence(Device& device);

    public:
        uint64_t GetCPUFrameIndex() const { return m_CPUFrameIndex; }
        uint64_t GetGPUFrameIndex() const { return m_GPUFrameIndex; }



    private:
        Device& m_Device;

        std::unique_ptr<Fence> m_Fence;
        uint64_t m_CPUFrameIndex = 0;
        uint64_t m_GPUFrameIndex = 0;
    };

    class SwapChain
    {
    public:
        BenzinDefineNonCopyable(SwapChain);
        BenzinDefineNonMoveable(SwapChain);

    public:
        SwapChain(const Window& window, const Backend& backend, Device& device);
        ~SwapChain();

    public:
        IDXGISwapChain3* GetDXGISwapChain() const { return m_DXGISwapChain; }

        uint32_t GetCurrentFrameIndex() const { return m_DXGISwapChain->GetCurrentBackBufferIndex(); }

        std::shared_ptr<Texture>& GetCurrentBackBuffer() { return m_BackBuffers[GetCurrentFrameIndex()]; }
        const std::shared_ptr<Texture>& GetCurrentBackBuffer() const { return m_BackBuffers[GetCurrentFrameIndex()]; }

        uint64_t GetCPUFrameIndex() const { return m_CPUFrameIndex; }
        uint64_t GetGPUFrameIndex() const { return m_GPUFrameIndex; }

        bool IsVSyncEnabled() const { return m_IsVSyncEnabled; }
        void SetVSyncEnabled(bool isEnabled) { m_IsVSyncEnabled = isEnabled; }

        float GetAspectRatio() const { return m_AspectRatio; }
        const Viewport& GetViewport() const { return m_Viewport; }
        const ScissorRect& GetScissorRect() const { return m_ScissorRect; }

    public:
        void SwapBackBuffer();

        void ResizeBackBuffers(uint32_t width, uint32_t height);

    private:
        void RegisterBackBuffers();
        void UpdateViewportDimensions(float width, float height);

        void FlushAndResetBackBuffers();

    private:
        Device& m_Device;

        IDXGISwapChain3* m_DXGISwapChain = nullptr;

        std::vector<std::shared_ptr<Texture>> m_BackBuffers;

        uint32_t m_DXGISwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        uint32_t m_DXGIPresentFlags = 0;

        bool m_IsVSyncEnabled = false;

        std::unique_ptr<Fence> m_FrameFence;
        uint64_t m_CPUFrameIndex = 0;
        uint64_t m_GPUFrameIndex = 0;

        float m_AspectRatio = 0.0f;
        Viewport m_Viewport;
        ScissorRect m_ScissorRect;
    };

} // namespace benzin
