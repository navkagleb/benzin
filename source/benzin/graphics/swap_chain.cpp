#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/swap_chain.hpp"

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
            .Format = (DXGI_FORMAT)GraphicsSettingsInstance::Get().BackBufferFormat,
            .Stereo = false,
            .SampleDesc{ 1, 0 },
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = GraphicsSettingsInstance::Get().FrameInFlightCount,
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
        BenzinAssert(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DXGISwapChain)));
        m_DXGISwapChain->SetMaximumFrameLatency(GraphicsSettingsInstance::Get().FrameInFlightCount);

        // Disable fullscreen using Alt + Enter
        BenzinAssert(backend.GetDXGIFactory()->MakeWindowAssociation(window.GetWin64Window(), DXGI_MWA_NO_ALT_ENTER));

        m_BackBuffers.resize(GraphicsSettingsInstance::Get().FrameInFlightCount);
        ResizeBackBuffers(window.GetWidth(), window.GetHeight());

        {
            m_FrameFence = std::make_unique<Fence>(m_Device);
            SetD3D12ObjectDebugName(m_FrameFence->GetD3D12Fence(), "SwapChainFrameFence");
        }
    }

    SwapChain::~SwapChain()
    {
        // Need to be released before 'm_DXGISwapChain' released
        FlushAndResetBackBuffers();

        SafeUnknownRelease(m_DXGISwapChain);
    }

    void SwapChain::ResizeBackBuffers(uint32_t width, uint32_t height)
    {
        FlushAndResetBackBuffers();

        BenzinAssert(m_DXGISwapChain->ResizeBuffers(
            (uint32_t)m_BackBuffers.size(),
            width,
            height,
            (DXGI_FORMAT)GraphicsSettingsInstance::Get().BackBufferFormat,
            m_DXGISwapChainFlags
        ));

        RegisterBackBuffers();
        UpdateViewportDimensions((float)width, (float)height);
    }

    void SwapChain::SwapBackBuffer()
    {
        BenzinAssert(m_DXGISwapChain->Present(m_IsVSyncEnabled, m_IsVSyncEnabled ? 0 : m_DXGIPresentFlags));

        m_Device.GetGraphicsCommandQueue().SignalFence(*m_FrameFence, m_CPUFrameIndex);

        // Update FrameFence
        m_CPUFrameIndex++;
        m_GPUFrameIndex = m_FrameFence->GetCompletedValue();

        if (m_CPUFrameIndex - m_GPUFrameIndex >= GraphicsSettingsInstance::Get().FrameInFlightCount)
        {
            m_GPUFrameIndex = m_CPUFrameIndex - GraphicsSettingsInstance::Get().FrameInFlightCount + 1;
            m_FrameFence->StallCurrentThreadUntilGPUCompletion(m_GPUFrameIndex);
        }
    }

    void SwapChain::RegisterBackBuffers()
    {
        BenzinAssert(!m_BackBuffers.empty());

        for (const auto& [i, backBuffer] : m_BackBuffers | std::views::enumerate)
        {
            ID3D12Resource* d3d12BackBuffer;
            BenzinAssert(m_DXGISwapChain->GetBuffer((uint32_t)i, IID_PPV_ARGS(&d3d12BackBuffer))); // Increases reference count
            
            SetD3D12ObjectDebugName(d3d12BackBuffer, "SwapChainBackBuffer", (uint32_t)i);

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

    void SwapChain::FlushAndResetBackBuffers()
    {
        m_Device.GetGraphicsCommandQueue().Flush(false);
        
        for (auto& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }
    }

} // namespace benzin
