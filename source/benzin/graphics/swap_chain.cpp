#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/swap_chain.hpp"

#include "benzin/core/cmd_line_args.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/cmd_queue.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/fence.hpp"
#include "benzin/graphics/d3d12_assert.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    SwapChain::SwapChain(const SwapChainCreation& creation)
        : m_Device{ creation.Device }
        , m_Width{ creation.Window.GetWidth() }
        , m_Height{ creation.Window.GetHeight() }
    {
        const HWND win64Window = creation.Window.GetWin64Window();
        const Backend& backend = creation.Backend;

        uint32_t isAllowTearing = 0;
        BenzinD3D12Call(backend.GetDxgiFactory()->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &isAllowTearing, sizeof(isAllowTearing)));

        uint32_t dxgiSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        if (isAllowTearing)
        {
            dxgiSwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        const DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1
        {
            .Width = m_Width,
            .Height = m_Height,
            .Format = (DXGI_FORMAT)GraphicsFormat::Rgba8Unorm,
            .Stereo = false,
            .SampleDesc{ 1, 0 },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = CmdLineArgs::GetFrameInFlightCount(),
            .Scaling = DXGI_SCALING_STRETCH,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
            .Flags = dxgiSwapChainFlags,
        };

        ComPtr<IDXGISwapChain1> dxgiSwapChain1;
        BenzinD3D12Call(backend.GetDxgiFactory()->CreateSwapChainForHwnd(
            m_Device.GetGraphicsCmdQueue().GetD3D12CommandQueue(),
            win64Window,
            &dxgiSwapChainDesc1,
            nullptr,
            nullptr,
            &dxgiSwapChain1
        ));
        BenzinD3D12Call(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DxgiSwapChain)));
        SetD3DObjectDebugName(m_DxgiSwapChain, creation.DebugName);

        // Disable fullscreen using Alt + Enter
        BenzinD3D12Call(backend.GetDxgiFactory()->MakeWindowAssociation(win64Window, DXGI_MWA_NO_ALT_ENTER));

        m_BackBuffers.resize(CmdLineArgs::GetFrameInFlightCount());
        ResizeBackBuffers();

        MakeUniquePtr(m_FrameFence, m_Device, FenceCreation
        {
            .DebugName = std::format("{}_FrameFence", creation.DebugName),
            .InitialValue = m_Device.m_CompletedGpuFrameIndex,
        });
    }

    SwapChain::~SwapChain()
    {
        ReleaseBackBuffers();
        SafeReleaseD3DObject(m_DxgiSwapChain);
    }

    Texture& SwapChain::GetCurrentBackBuffer()
    {
        return *m_BackBuffers[m_Device.GetActiveFrameIndex()];
    }

    const Texture& SwapChain::GetCurrentBackBuffer() const
    {
        return *m_BackBuffers[m_Device.GetActiveFrameIndex()];
    }

    bool SwapChain::OnFlip(bool isVerticalSyncEnabled)
    {
        BenzinProfile();

        uint64_t cpuFrameIndex = m_Device.m_CpuFrameIndex;
        uint64_t gpuFrameIndex = m_Device.m_CompletedGpuFrameIndex;

        {
            cpuFrameIndex++;
            m_Device.GetGraphicsCmdQueue().SignalFence(*m_FrameFence, cpuFrameIndex);
        }

        {
            BenzinScopeProfile("SwapChain Present");
            BenzinD3D12Call(m_DxgiSwapChain->Present(isVerticalSyncEnabled, 0));
        }

        {
            gpuFrameIndex = m_FrameFence->GetCompletedValue();

            if (cpuFrameIndex - gpuFrameIndex >= CmdLineArgs::GetFrameInFlightCount())
            {
                {
                    BenzinScopeProfile("SwapChain WaitForGpu");

                    const uint64_t gpuFrameIndexToWait = cpuFrameIndex - CmdLineArgs::GetFrameInFlightCount() + 1;
                    m_FrameFence->StopCurrentThreadBeforeGpuFinish(gpuFrameIndexToWait);
                }

                // 'm_FrameFence' completed value may differ from 'gpuFrameIndexToWait'
                // Therefore, save 'm_FrameFence' completed value because it's may be updated during the waiting time
                gpuFrameIndex = m_FrameFence->GetCompletedValue();
            }
        }

        BenzinExecuteOnScopeExit([&]
        {
            m_Device.m_CpuFrameIndex = cpuFrameIndex;
            m_Device.m_CompletedGpuFrameIndex = gpuFrameIndex;
            m_Device.m_ActiveFrameIndex = (uint8_t)m_DxgiSwapChain->GetCurrentBackBufferIndex();
        });

        DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
        BenzinD3D12Call(m_DxgiSwapChain->GetDesc1(&dxgiSwapChainDesc));
        if (m_Width != dxgiSwapChainDesc.Width || m_Height != dxgiSwapChainDesc.Height)
        {
            BenzinLogTimeOnScopeExit(
                "SwapChain ResizeBuffers from ({} x {}) to ({} x {})",
                dxgiSwapChainDesc.Width, dxgiSwapChainDesc.Height,
                m_Width, m_Height
            );

            m_Device.GetGraphicsCmdQueue().Flush();

            ReleaseBackBuffers();
            ResizeBackBuffers();

            return true;
        }

        return false;
    }

    void SwapChain::RequestResize(uint32_t width, uint32_t height)
    {
        BenzinAssert(width != 0 && height != 0);

        m_Width = width;
        m_Height = height;
    }

    void SwapChain::RegisterBackBuffers()
    {
        for (const auto& [i, backBuffer] : m_BackBuffers | std::views::enumerate)
        {
            ID3D12Resource* d3d12BackBuffer;
            BenzinD3D12Call(m_DxgiSwapChain->GetBuffer((uint32_t)i, IID_PPV_ARGS(&d3d12BackBuffer))); // Increases reference count
            
            MakeUniquePtr(backBuffer, m_Device, d3d12BackBuffer);
            SetD3DObjectDebugName(d3d12BackBuffer, std::format("SwapChainBackBuffer{}", i));
        }
    }

    void SwapChain::ReleaseBackBuffers()
    {
        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }

        m_Device.ProcessDeferredReleaseQueues(true);
    }

    void SwapChain::ResizeBackBuffers()
    {
#if BENZIN_IS_ASSERTS_ENABLED
        for (const auto& backBuffer : m_BackBuffers)
        {
            BenzinAssert(backBuffer.get() == nullptr);
        }
#endif

        DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
        BenzinD3D12Call(m_DxgiSwapChain->GetDesc1(&dxgiSwapChainDesc));

        BenzinD3D12Call(m_DxgiSwapChain->ResizeBuffers(
            dxgiSwapChainDesc.BufferCount,
            m_Width,
            m_Height,
            dxgiSwapChainDesc.Format,
            dxgiSwapChainDesc.Flags
        ));
        RegisterBackBuffers();
    }

}
