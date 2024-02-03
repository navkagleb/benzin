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

        Texture& GetCurrentBackBuffer();
        const Texture& GetCurrentBackBuffer() const;

        bool IsVSyncEnabled() const { return m_IsVSyncEnabled; }
        void SetVSyncEnabled(bool isEnabled) { m_IsVSyncEnabled = isEnabled; }

        float GetAspectRatio() const { return m_AspectRatio; }
        const Viewport& GetViewport() const { return m_Viewport; }
        const ScissorRect& GetScissorRect() const { return m_ScissorRect; }

    public:
        void Flip();

        void ResizeBackBuffers(uint32_t width, uint32_t height);

    private:
        void RegisterBackBuffers();
        void UpdateViewportDimensions(float width, float height);

        void FlushAndResetBackBuffers();

    private:
        Device& m_Device;

        IDXGISwapChain3* m_DXGISwapChain = nullptr;

        uint32_t m_DXGISwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        uint32_t m_DXGIPresentFlags = 0;

        std::vector<std::unique_ptr<Texture>> m_BackBuffers;

        std::unique_ptr<Fence> m_FrameFence;

        bool m_IsVSyncEnabled = false;

        float m_AspectRatio = 0.0f;
        Viewport m_Viewport;
        ScissorRect m_ScissorRect;
    };

} // namespace benzin
