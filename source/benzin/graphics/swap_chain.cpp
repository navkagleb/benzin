#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/swap_chain.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/d3d12_debug.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/system/window.hpp>

namespace benzin
{

    SwapChain::SwapChain(const SwapChainCreation& creation)
        : m_Device{ creation.m_Device }
    {
        uint32_t isTearingSupported = 0;
        BenzinD3D12Call(creation.m_Backend.GetDxgiFactory()->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &isTearingSupported,
            sizeof(isTearingSupported)));

        uint32_t dxgiSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        if (isTearingSupported)
        {
            dxgiSwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            m_DxgiPresentFlags |= DXGI_PRESENT_ALLOW_TEARING;
        }

        DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc = {};
        dxgiSwapChainDesc.Width = creation.m_Window.GetWidth();
        dxgiSwapChainDesc.Height = creation.m_Window.GetHeight();
        dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        dxgiSwapChainDesc.Stereo = false;
        dxgiSwapChainDesc.SampleDesc = { 1, 0 };
        dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        dxgiSwapChainDesc.BufferCount = BENZIN_FRAME_COUNT;
        dxgiSwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        dxgiSwapChainDesc.Flags = dxgiSwapChainFlags;

        ComPtr<IDXGISwapChain1> dxgiSwapChain1;
        BenzinD3D12Call(creation.m_Backend.GetDxgiFactory()->CreateSwapChainForHwnd(
            creation.m_Device.GetGraphicsCmdQueue().GetD3D12CommandQueue(),
            creation.m_Window.GetWin64Window(),
            &dxgiSwapChainDesc,
            nullptr,
            nullptr,
            &dxgiSwapChain1));

        BenzinD3D12Call(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DxgiSwapChain)));
        SetD3DObjectDebugName(m_DxgiSwapChain, creation.m_DebugName);

        m_DxgiSwapChain->SetMaximumFrameLatency(BENZIN_FRAME_COUNT - 1);

        // Disable fullscreen using Alt + Enter
        BenzinD3D12Call(creation.m_Backend.GetDxgiFactory()->MakeWindowAssociation(
            creation.m_Window.GetWin64Window(),
            DXGI_MWA_NO_ALT_ENTER));

        RegisterBackBuffers();
    }

    SwapChain::~SwapChain()
    {
        ReleaseBackBuffers();
        SafeReleaseD3DObject(m_DxgiSwapChain);
    }

    void SwapChain::Flip(bool isVsyncEnabled)
    {
        BenzinProfile();

        BenzinD3D12Call(m_DxgiSwapChain->Present(isVsyncEnabled, isVsyncEnabled ? 0 : m_DxgiPresentFlags));
    }

    void SwapChain::Resize(uint32_t width, uint32_t height)
    {
        DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc = {};
        BenzinD3D12Call(m_DxgiSwapChain->GetDesc1(&dxgiSwapChainDesc));

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

    void SwapChain::ReleaseBackBuffers()
    {
        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }
    }

}
