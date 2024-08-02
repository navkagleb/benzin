#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Window;

    class Backend;
    class Device;
    class Fence;
    class Texture;

    struct SwapChainCreation
    {
        std::string_view DebugName;

        const Window& WindowRef;
        Device& DeviceRef;
    };

    class SwapChain
    {
    public:
        explicit SwapChain(const SwapChainCreation& creation);
        ~SwapChain();

        BenzinDefineNonCopyable(SwapChain);
        BenzinDefineNonMoveable(SwapChain);

    public:
        Texture& GetCurrentBackBuffer();
        const Texture& GetCurrentBackBuffer() const;

        auto GetWidth() const { return m_Width; }
        auto GetHeight() const { return m_Height; }

        auto GetPresentTime() const { return m_PresentTime; }
        auto GetGpuWaitTime() const { return m_GpuWaitTime; }

    public:
        bool OnFlip(bool isVerticalSyncEnabled);
        void RequestResize(uint32_t width, uint32_t height);

    private:
        void RegisterBackBuffers();
        void ReleaseBackBuffers();
        void ResizeBackBuffers();

    private:
        Device& m_Device;

        IDXGISwapChain3* m_DxgiSwapChain = nullptr;
        std::vector<std::unique_ptr<Texture>> m_BackBuffers;
        std::unique_ptr<Fence> m_FrameFence;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;

        std::chrono::microseconds m_PresentTime = std::chrono::microseconds::zero();
        std::chrono::microseconds m_GpuWaitTime = std::chrono::microseconds::zero();
    };

} // namespace benzin
