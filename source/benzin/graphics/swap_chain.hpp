#pragma once

namespace benzin
{

    class Backend;
    class Device;
    class Texture;
    class Window;

    struct SwapChainCreation
    {
        std::string_view m_DebugName;

        const Window& m_Window;
        const Backend& m_Backend;
        Device& m_Device;
    };

    class SwapChain
    {
    public:
        explicit SwapChain(const SwapChainCreation& creation);
        ~SwapChain();

        BenzinDefineNonCopyable(SwapChain);
        BenzinDefineNonMoveable(SwapChain);

        uint32_t GetCurrentBackBufferIndex() const { return m_DxgiSwapChain->GetCurrentBackBufferIndex(); }
        const auto& GetCurrentBackBuffer() const { return *m_BackBuffers[GetCurrentBackBufferIndex()]; }

        void Flip(bool isVsyncEnabled);
        void Resize(uint32_t width, uint32_t height);

    private:
        void RegisterBackBuffers();
        void ReleaseBackBuffers(bool isForceRelease);

        Device& m_Device;

        IDXGISwapChain3* m_DxgiSwapChain = nullptr;
        std::unique_ptr<Texture> m_BackBuffers[BENZIN_FRAME_COUNT];

        uint32_t m_DxgiPresentFlags = 0;
    };

}
