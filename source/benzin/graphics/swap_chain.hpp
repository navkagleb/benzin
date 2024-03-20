#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Window;

    class Backend;
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
        Texture& GetCurrentBackBuffer();
        const Texture& GetCurrentBackBuffer() const;

        auto GetAspectRatio() const { return m_AspectRatio; }
        const auto& GetViewport() const { return m_Viewport; }
        const auto& GetScissorRect() const { return m_ScissorRect; }

        auto GetViewportWidth() const { return (uint32_t)m_Viewport.Width; }
        auto GetViewportHeight() const { return (uint32_t)m_Viewport.Height; }

    public:
        void OnFlip(bool isVerticalSyncEnabled);
        void RequestResize(uint32_t width, uint32_t height);

    private:
        void RegisterBackBuffers();
        void ReleaseBackBuffers();
        void ResizeBackBuffers(uint32_t width, uint32_t height);

        void UpdateViewportDimensions(float width, float height);

    private:
        Device& m_Device;

        IDXGISwapChain3* m_DxgiSwapChain = nullptr;
        std::vector<std::unique_ptr<Texture>> m_BackBuffers;
        std::unique_ptr<Fence> m_FrameFence;

        uint32_t m_PendingWidth = 0;
        uint32_t m_PendingHeight = 0;

        float m_AspectRatio = 0.0f;
        Viewport m_Viewport;
        ScissorRect m_ScissorRect;
    };

} // namespace benzin
