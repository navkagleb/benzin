#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/swap_chain.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/system/window.hpp>

namespace benzin
{

    static bool IsTearingSupported(const Backend& backend)
    {
        uint32_t isTearingSupported = 0;
        BenzinD3D12Call(backend.GetDxgiFactory()->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &isTearingSupported,
            sizeof(isTearingSupported)));

        return isTearingSupported;
    }

    static uint32_t GetDxgiSwapChainFlags(const Backend& backend)
    {
        uint32_t flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        if (IsTearingSupported(backend))
        {
            flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        return flags;
    }

    //

    SwapChain::SwapChain(const SwapChainCreation& creation)
        : m_Device{ creation.m_Device }
    {
        const uint32_t width = creation.m_Window.GetWidth();
        const uint32_t height = creation.m_Window.GetHeight();

        const DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1
        {
            .Width = width,
            .Height = height,
            .Format = (DXGI_FORMAT)GraphicsFormat::Rgba8Unorm,
            .Stereo = false,
            .SampleDesc{ 1, 0 },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = BENZIN_FRAME_COUNT,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = GetDxgiSwapChainFlags(creation.m_Backend),
        };

        ComPtr<IDXGISwapChain1> dxgiSwapChain1;
        BenzinD3D12Call(creation.m_Backend.GetDxgiFactory()->CreateSwapChainForHwnd(
            creation.m_Device.GetGraphicsCmdQueue().GetD3D12CommandQueue(),
            creation.m_Window.GetWin64Window(),
            &dxgiSwapChainDesc1,
            nullptr,
            nullptr,
            &dxgiSwapChain1));

        BenzinD3D12Call(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DxgiSwapChain)));
        SetD3DObjectDebugName(m_DxgiSwapChain, creation.m_DebugName);

        // Disable fullscreen using Alt + Enter
        BenzinD3D12Call(creation.m_Backend.GetDxgiFactory()->MakeWindowAssociation(
            creation.m_Window.GetWin64Window(),
            DXGI_MWA_NO_ALT_ENTER));

        RegisterBackBuffers();
    }

    SwapChain::~SwapChain()
    {
        constexpr bool isForceRelease = false;
        ReleaseBackBuffers(isForceRelease);
        SafeReleaseD3DObject(m_DxgiSwapChain);
    }

    void SwapChain::Flip(bool isVerticalSyncEnabled)
    {
        BenzinProfile();

        BenzinD3D12Call(m_DxgiSwapChain->Present(isVerticalSyncEnabled, 0));
    }

    void SwapChain::Resize(uint32_t width, uint32_t height)
    {
        DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc = {};
        BenzinD3D12Call(m_DxgiSwapChain->GetDesc1(&dxgiSwapChainDesc));

        constexpr bool isForceRelease = true;
        ReleaseBackBuffers(isForceRelease);

        BenzinD3D12Call(m_DxgiSwapChain->ResizeBuffers(
            dxgiSwapChainDesc.BufferCount,
            width,
            height,
            dxgiSwapChainDesc.Format,
            dxgiSwapChainDesc.Flags));

        RegisterBackBuffers();
    }

    void SwapChain::RegisterBackBuffers()
    {
        for (uint32_t i = 0; i < BENZIN_FRAME_COUNT; ++i)
        {
            ID3D12Resource* d3d12BackBuffer;
            BenzinD3D12Call(m_DxgiSwapChain->GetBuffer((uint32_t)i, IID_PPV_ARGS(&d3d12BackBuffer))); // Increases reference count

            MakeUniquePtr(m_BackBuffers[i], m_Device, d3d12BackBuffer);
            SetD3DObjectDebugName(d3d12BackBuffer, std::format("SwapChainBackBuffer{}", i));
        }
    }

    void SwapChain::ReleaseBackBuffers(bool isForceRelease)
    {
        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }

        if (isForceRelease)
        {
            m_Device.ProcessDeferredReleaseQueues(true);
        }
    }

}
