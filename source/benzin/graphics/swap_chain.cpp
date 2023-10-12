#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/swap_chain.hpp"

#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    SwapChain::SwapChain(const Window& window, const Backend& backend, Device& device)
        : m_Device{ device }
        , m_FrameFence{ device }
    {
        SetD3D12ObjectDebugName(m_FrameFence.GetD3D12Fence(), "SwapChainFrame");

        m_DXGISwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        uint32_t isAllowTearing = 0;
        BenzinAssert(backend.GetDXGIFactory()->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &isAllowTearing, sizeof(isAllowTearing)));

        if (isAllowTearing)
        {
            m_DXGISwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            m_DXGIPresentFlags |= DXGI_PRESENT_ALLOW_TEARING;
        }

        const DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1
        {
            .Width = window.GetWidth(),
            .Height = window.GetHeight(),
            .Format = static_cast<DXGI_FORMAT>(config::g_BackBufferFormat),
            .Stereo = false,
            .SampleDesc{ 1, 0 },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = static_cast<UINT>(config::g_BackBufferCount),
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = m_DXGISwapChainFlags,
        };

        ComPtr<IDXGISwapChain1> dxgiSwapChain1;
        BenzinAssert(backend.GetDXGIFactory()->CreateSwapChainForHwnd(
            m_Device.GetGraphicsCommandQueue().GetD3D12CommandQueue(),
            window.GetWin64Window(),
            &dxgiSwapChainDesc1,
            nullptr,
            nullptr,
            &dxgiSwapChain1
        ));

        // Disable fullscreen using Alt + Enter
        BenzinAssert(backend.GetDXGIFactory()->MakeWindowAssociation(window.GetWin64Window(), DXGI_MWA_NO_ALT_ENTER));

        BenzinAssert(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DXGISwapChain)));

        m_DXGISwapChain->SetMaximumFrameLatency(config::g_BackBufferCount);

        ResizeBackBuffers(window.GetWidth(), window.GetHeight());
    }

    SwapChain::~SwapChain()
    {
        m_Device.GetGraphicsCommandQueue().Flush();

        // Need to be released before 'm_DXGISwapChain' released
        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }

        SafeUnknownRelease(m_DXGISwapChain);
    }

    void SwapChain::ResizeBackBuffers(uint32_t width, uint32_t height)
    {
        m_Device.GetGraphicsCommandQueue().Flush();

        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }

        BenzinAssert(m_DXGISwapChain->ResizeBuffers(
            static_cast<UINT>(m_BackBuffers.size()),
            width,
            height,
            static_cast<DXGI_FORMAT>(config::g_BackBufferFormat),
            m_DXGISwapChainFlags
        ));

        RegisterBackBuffers();
        UpdateViewportDimensions(static_cast<float>(width), static_cast<float>(height));
    }

    void SwapChain::Flip()
    {
        m_Device.GetGraphicsCommandQueue().UpdateFenceValue(m_FrameFence, ++m_CPUFrameIndex);

        BenzinAssert(m_DXGISwapChain->Present(m_IsVSyncEnabled, m_IsVSyncEnabled ? 0 : m_DXGIPresentFlags));

        m_GPUFrameIndex = m_FrameFence.GetCompletedValue();

        WaitForGPU();
    }

    void SwapChain::RegisterBackBuffers()
    {
        for (size_t i = 0; i < m_BackBuffers.size(); ++i)
        {
            ID3D12Resource* d3d12BackBuffer;
            BenzinAssert(m_DXGISwapChain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&d3d12BackBuffer))); // Increases reference count
            
            SetD3D12ObjectDebugName(d3d12BackBuffer, "SwapChainBackBuffer", static_cast<uint32_t>(i));

            auto& backBuffer = m_BackBuffers[i];
            backBuffer = std::make_shared<Texture>(m_Device, d3d12BackBuffer);
            backBuffer->PushRenderTargetView();
        }
    }

    void SwapChain::UpdateViewportDimensions(float width, float height)
    {
        m_AspectRatio = width / height;

        m_Viewport.Width = width;
        m_Viewport.Height = height;

        m_ScissorRect.Width = width;
        m_ScissorRect.Height = height;
    }

    void SwapChain::WaitForGPU()
    {
        const uint64_t frameDistance = m_CPUFrameIndex - m_GPUFrameIndex;

        if (frameDistance >= config::g_BackBufferCount)
        {
            const uint64_t gpuFrameIndexToWait = m_CPUFrameIndex - config::g_BackBufferCount + 1;
            m_FrameFence.WaitForGPU(gpuFrameIndexToWait);

            m_GPUFrameIndex = m_FrameFence.GetCompletedValue();
        }
    }

} // namespace benzin
