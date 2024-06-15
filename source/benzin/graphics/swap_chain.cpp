#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/swap_chain.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/fence.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    SwapChain::SwapChain(const SwapChainCreation& creation)
        : m_Device{ creation.DeviceRef }
    {
        const uint32_t frameInFlightCount = CommandLineArgs::GetFrameInFlightCount();

        uint32_t isAllowTearing = 0;
        BenzinEnsure(creation.BackendRef.GetDxgiFactory()->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &isAllowTearing, sizeof(isAllowTearing)));

        uint32_t dxgiSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        if (isAllowTearing)
        {
            dxgiSwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        const DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1
        {
            .Width = creation.WindowRef.GetWidth(),
            .Height = creation.WindowRef.GetHeight(),
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
        BenzinEnsure(creation.BackendRef.GetDxgiFactory()->CreateSwapChainForHwnd(
            m_Device.GetGraphicsCommandQueue().GetD3D12CommandQueue(),
            creation.WindowRef.GetWin64Window(),
            &dxgiSwapChainDesc1,
            nullptr,
            nullptr,
            &dxgiSwapChain1
        ));
        BenzinEnsure(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DxgiSwapChain)));
        SetDxObjectDebugName(m_DxgiSwapChain, creation.DebugName);

        // Disable fullscreen using Alt + Enter
        BenzinEnsure(creation.BackendRef.GetDxgiFactory()->MakeWindowAssociation(creation.WindowRef.GetWin64Window(), DXGI_MWA_NO_ALT_ENTER));

        m_BackBuffers.resize(frameInFlightCount);
        ResizeBackBuffers(creation.WindowRef.GetWidth(), creation.WindowRef.GetHeight());

        MakeUniquePtr(m_FrameFence, m_Device, FenceCreation
        {
            .DebugName = std::format("{}_FrameFence", creation.DebugName),
            .InitialValue = m_Device.m_CompletedGpuFrameIndex,
        });
    }

    SwapChain::~SwapChain()
    {
        ReleaseBackBuffers();
        m_Device.ProcessDeferredReleaseQueues(true); // TODO: Create SwapChain from Device
        BenzinSafeDxObjectRelease(m_DxgiSwapChain);
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
        uint64_t gpuFrameIndex = m_Device.m_CompletedGpuFrameIndex;

        {
            cpuFrameIndex++;
            m_Device.GetGraphicsCommandQueue().SignalFence(*m_FrameFence, cpuFrameIndex);
        }

        {
            BenzinGrabTimeOnScopeExit(m_PresentTime);
            BenzinEnsure(m_DxgiSwapChain->Present(isVerticalSyncEnabled, 0));
        }

        {
            gpuFrameIndex = m_FrameFence->GetCompletedValue();

            if (cpuFrameIndex - gpuFrameIndex >= frameInFlightCount)
            {
                {
                    BenzinGrabTimeOnScopeExit(m_GpuWaitTime);

                    const uint64_t gpuFrameIndexToWait = cpuFrameIndex - frameInFlightCount + 1;
                    m_FrameFence->StopCurrentThreadBeforeGpuFinish(gpuFrameIndexToWait);
                }

                // 'm_FrameFence' completed value may differ from 'gpuFrameIndexToWait'
                // Therefore, save 'm_FrameFence' completed value because it's may be updated during the waiting time
                gpuFrameIndex = m_FrameFence->GetCompletedValue();
            }
            else
            {
                m_GpuWaitTime = std::chrono::microseconds::zero();
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
            ReleaseBackBuffers();
            m_Device.ProcessDeferredReleaseQueues(true);

            ResizeBackBuffers(m_PendingWidth, m_PendingHeight);

            m_PendingWidth = 0;
            m_PendingHeight = 0;
        }

        m_Device.m_CpuFrameIndex = cpuFrameIndex;
        m_Device.m_CompletedGpuFrameIndex = gpuFrameIndex;
        m_Device.m_ActiveFrameIndex = (uint8_t)m_DxgiSwapChain->GetCurrentBackBufferIndex();
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
            SetDxObjectDebugName(d3d12BackBuffer, std::format("SwapChainBackBuffer{}", i));
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
#if BENZIN_IS_ASSERTS_ENABLED
        for (const auto& backBuffer : m_BackBuffers)
        {
            BenzinAssert(backBuffer.get() == nullptr);
        }
#endif

        DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
        BenzinEnsure(m_DxgiSwapChain->GetDesc1(&dxgiSwapChainDesc));

        BenzinEnsure(m_DxgiSwapChain->ResizeBuffers(
            dxgiSwapChainDesc.BufferCount,
            width,
            height,
            dxgiSwapChainDesc.Format,
            dxgiSwapChainDesc.Flags
        ));
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
