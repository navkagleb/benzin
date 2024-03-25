#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/swap_chain.hpp"

#include "benzin/core/command_line_args.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/fence.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    SwapChain::SwapChain(const Window& window, const Backend& backend, Device& device)
        : m_Device{ device }
    {
        const uint32_t frameInFlightCount = CommandLineArgs::GetFrameInFlightCount();

        uint32_t isAllowTearing = 0;
        BenzinEnsure(backend.GetDxgiFactory()->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &isAllowTearing, sizeof(isAllowTearing)));

        uint32_t dxgiSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        if (isAllowTearing)
        {
            dxgiSwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        const DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1
        {
            .Width = window.GetWidth(),
            .Height = window.GetHeight(),
            .Format = (DXGI_FORMAT)CommandLineArgs::GetBackBufferFormat(),
            .Stereo = false,
            .SampleDesc{ 1, 0 },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = frameInFlightCount,
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = dxgiSwapChainFlags,
        };

        ComPtr<IDXGISwapChain1> dxgiSwapChain1;
        BenzinEnsure(backend.GetDxgiFactory()->CreateSwapChainForHwnd(
            m_Device.GetGraphicsCommandQueue().GetD3D12CommandQueue(),
            window.GetWin64Window(),
            &dxgiSwapChainDesc1,
            nullptr,
            nullptr,
            &dxgiSwapChain1
        ));
        BenzinEnsure(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DxgiSwapChain)));

        // Disable fullscreen using Alt + Enter
        BenzinEnsure(backend.GetDxgiFactory()->MakeWindowAssociation(window.GetWin64Window(), DXGI_MWA_NO_ALT_ENTER));

        m_BackBuffers.resize(frameInFlightCount);
        ResizeBackBuffers(window.GetWidth(), window.GetHeight());

        MakeUniquePtr(m_FrameFence, m_Device, "SwapChainFrameFence");
    }

    SwapChain::~SwapChain()
    {
        m_Device.GetGraphicsCommandQueue().SumbitCommandList();
        m_Device.GetGraphicsCommandQueue().Flush();

        ReleaseBackBuffers();
        SafeUnknownRelease(m_DxgiSwapChain);
    }

    Texture& SwapChain::GetCurrentBackBuffer()
    {
        return *m_BackBuffers[m_Device.GetActiveFrameIndex()];
    }

    const Texture& SwapChain::GetCurrentBackBuffer() const
    {
        return *m_BackBuffers[m_Device.GetActiveFrameIndex()];
    }

    void SwapChain::OnFlip(bool isVerticalSyncEnabled)
    {
        const uint32_t frameInFlightCount = CommandLineArgs::GetFrameInFlightCount();

        uint64_t cpuFrameIndex = m_Device.m_CpuFrameIndex;
        uint64_t gpuFrameIndex = m_Device.m_GpuFrameIndex;

        {
            cpuFrameIndex++;
            m_Device.GetGraphicsCommandQueue().SignalFence(*m_FrameFence, cpuFrameIndex);
        }

        {
            BenzinEnsure(m_DxgiSwapChain->Present(isVerticalSyncEnabled, 0));
        }

        {
            gpuFrameIndex = m_FrameFence->GetCompletedValue();

            if (cpuFrameIndex - gpuFrameIndex >= frameInFlightCount)
            {
                const uint64_t gpuFrameIndexToWait = cpuFrameIndex - frameInFlightCount + 1;
                m_FrameFence->StopCurrentThreadBeforeGpuFinish(gpuFrameIndexToWait);

                // 'm_FrameFence' completed value may differ from 'gpuFrameIndexToWait'
                // Therefore, save 'm_FrameFence' completed value because it's may be updated during the waiting time
                gpuFrameIndex = m_FrameFence->GetCompletedValue();
            }
        }
        
        DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
        BenzinEnsure(m_DxgiSwapChain->GetDesc1(&dxgiSwapChainDesc));
        if (m_PendingWidth != 0 && m_PendingHeight != 0 && (m_PendingWidth != dxgiSwapChainDesc.Width || m_PendingHeight != dxgiSwapChainDesc.Height))
        {
            BenzinLogTimeOnScopeExit(
                "SwapChain ResizeBuffers from ({} x {}) to ({} x {})",
                dxgiSwapChainDesc.Width, dxgiSwapChainDesc.Height,
                m_PendingWidth, m_PendingHeight
            );

            m_Device.GetGraphicsCommandQueue().Flush();
            ResizeBackBuffers(m_PendingWidth, m_PendingHeight);

            m_PendingWidth = 0;
            m_PendingHeight = 0;
        }

        m_Device.m_CpuFrameIndex = cpuFrameIndex;
        m_Device.m_GpuFrameIndex = gpuFrameIndex;
        m_Device.m_ActiveFrameIndex = m_DxgiSwapChain->GetCurrentBackBufferIndex();
    }

    void SwapChain::RequestResize(uint32_t width, uint32_t height)
    {
        BenzinAssert(width != 0 && height != 0);

        m_PendingWidth = width;
        m_PendingHeight = height;
    }

    void SwapChain::RegisterBackBuffers()
    {
        for (const auto& [i, backBuffer] : m_BackBuffers | std::views::enumerate)
        {
            ID3D12Resource* d3d12BackBuffer;
            BenzinEnsure(m_DxgiSwapChain->GetBuffer((uint32_t)i, IID_PPV_ARGS(&d3d12BackBuffer))); // Increases reference count
            
            MakeUniquePtr(backBuffer, m_Device, d3d12BackBuffer);
            SetD3D12ObjectDebugName(d3d12BackBuffer, "SwapChainBackBuffer", (uint32_t)i);
        }
    }

    void SwapChain::ReleaseBackBuffers()
    {
        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }
    }

    void SwapChain::ResizeBackBuffers(uint32_t width, uint32_t height)
    {
        ReleaseBackBuffers();
        {
            DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
            BenzinEnsure(m_DxgiSwapChain->GetDesc1(&dxgiSwapChainDesc));

            BenzinEnsure(m_DxgiSwapChain->ResizeBuffers(
                dxgiSwapChainDesc.BufferCount,
                width,
                height,
                dxgiSwapChainDesc.Format,
                dxgiSwapChainDesc.Flags
            ));
        }
        RegisterBackBuffers();

        UpdateViewportDimensions((float)width, (float)height);
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
