#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/swap_chain.hpp"

#include "benzin/core/common.hpp"
#include "benzin/graphics/api/backend.hpp"
#include "benzin/graphics/api/command_queue.hpp"
#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/texture.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    SwapChain::SwapChain(const Window& window, const Backend& backend, Device& device)
        : m_Device{ device }
        , m_FrameFence{ device }
    {
        m_FrameFence.SetDebugName("SwapChainFrame");

        m_DXGISwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        uint32_t isAllowTearing = 0;
        BENZIN_HR_ASSERT(backend.GetDXGIFactory()->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &isAllowTearing, sizeof(isAllowTearing)));

        if (isAllowTearing)
        {
            m_DXGISwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            m_DXGIPresentFlags |= DXGI_PRESENT_ALLOW_TEARING;
        }

        const DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1
        {
            .Width{ window.GetWidth() },
            .Height{ window.GetHeight() },
            .Format{ static_cast<DXGI_FORMAT>(config::GetBackBufferFormat()) },
            .Stereo{ false },
            .SampleDesc{ 1, 0 },
            .BufferUsage{ DXGI_USAGE_RENDER_TARGET_OUTPUT },
            .BufferCount{ static_cast<UINT>(config::GetBackBufferCount()) },
            .Scaling{ DXGI_SCALING_STRETCH },
            .SwapEffect{ DXGI_SWAP_EFFECT_FLIP_DISCARD },
            .AlphaMode{ DXGI_ALPHA_MODE_UNSPECIFIED },
            .Flags{ m_DXGISwapChainFlags },
        };

        ComPtr<IDXGISwapChain1> dxgiSwapChain1;
        BENZIN_HR_ASSERT(backend.GetDXGIFactory()->CreateSwapChainForHwnd(
            m_Device.GetGraphicsCommandQueue().GetD3D12CommandQueue(),
            window.GetWin64Window(),
            &dxgiSwapChainDesc1,
            nullptr,
            nullptr,
            &dxgiSwapChain1
        ));

        // Disable fullscreen using Alt + Enter
        BENZIN_HR_ASSERT(backend.GetDXGIFactory()->MakeWindowAssociation(window.GetWin64Window(), DXGI_MWA_NO_ALT_ENTER));

        BENZIN_HR_ASSERT(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DXGISwapChain)));

        m_DXGISwapChain->SetMaximumFrameLatency(config::GetBackBufferCount());

        ResizeBackBuffers(window.GetWidth(), window.GetHeight());
    }

    SwapChain::~SwapChain()
    {
        m_Device.GetGraphicsCommandQueue().Flush();

        // Need to be released before m_DXGISwapChain released
        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }

        dx::SafeRelease(m_DXGISwapChain);
    }

    void SwapChain::ResizeBackBuffers(uint32_t width, uint32_t height)
    {
        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }

        BENZIN_HR_ASSERT(m_DXGISwapChain->ResizeBuffers(
            static_cast<UINT>(m_BackBuffers.size()),
            width,
            height,
            static_cast<DXGI_FORMAT>(config::GetBackBufferFormat()),
            m_DXGISwapChainFlags
        ));

        RegisterBackBuffers();
        UpdateViewportDimensions(static_cast<float>(width), static_cast<float>(height));
    }

    void SwapChain::Flip()
    {
        m_Device.GetGraphicsCommandQueue().UpdateFenceValue(m_FrameFence, ++m_CPUFrameIndex);

        BENZIN_HR_ASSERT(m_DXGISwapChain->Present(m_IsVSyncEnabled, m_DXGIPresentFlags));

        m_GPUFrameIndex = m_FrameFence.GetCompletedValue();

        // Wait for GPU to not exceed maximum queued frames
        const uint64_t frameDistance = m_CPUFrameIndex - m_GPUFrameIndex;
        if (frameDistance >= config::GetBackBufferCount())
        {
            const uint64_t gpuFrameIndexToWait = m_CPUFrameIndex - config::GetBackBufferCount() + 1;
            m_FrameFence.WaitForGPU(gpuFrameIndexToWait);

            m_GPUFrameIndex = m_FrameFence.GetCompletedValue();
        }
    }

    void SwapChain::RegisterBackBuffers()
    {
        for (size_t i = 0; i < m_BackBuffers.size(); ++i)
        {
            ID3D12Resource* d3d12BackBuffer;
            BENZIN_HR_ASSERT(m_DXGISwapChain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&d3d12BackBuffer))); // Increases reference count
            
            auto& backBuffer = m_BackBuffers[i];
            backBuffer = std::make_shared<TextureResource>(m_Device, d3d12BackBuffer);
            backBuffer->SetDebugName(fmt::format("SwapChainBackBuffer_{}", i));
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

} // namespace benzin
