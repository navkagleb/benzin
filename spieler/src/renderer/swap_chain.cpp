#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/swap_chain.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/context.hpp"

namespace spieler::renderer
{

    SwapChain::SwapChain(Device& device, Context& context, Window& window, const SwapChainConfig& props)
        : m_BufferCount{ props.BufferCount }
        , m_BufferFormat{ props.BufferFormat }
        , m_DepthStencilFormat{ props.DepthStencilFormat }
    {
        SPIELER_ASSERT(Init(device, context, window));
    }

    SwapChain::~SwapChain()
    {
#if defined(SPIELER_DEBUG)
        ComPtr<IDXGIDebug1> dxgiDebug;

        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
#endif
    }

    const Texture2D& SwapChain::GetCurrentBuffer() const
    {
        const uint32_t bufferIndex{ m_SwapChain3->GetCurrentBackBufferIndex() };

        return m_Buffers[bufferIndex];
    }

    bool SwapChain::Init(Device& device, Context& context, Window& window)
    {
        SPIELER_RETURN_IF_FAILED(InitFactory());
        SPIELER_RETURN_IF_FAILED(InitSwapChain(context, window));

        m_Buffers.resize(m_BufferCount);

        SPIELER_RETURN_IF_FAILED(ResizeBuffers(device, context, window.GetWidth(), window.GetHeight()));

        return true;
    }

    bool SwapChain::ResizeBuffers(Device& device, Context& context, uint32_t width, uint32_t height)
    {
        SPIELER_RETURN_IF_FAILED(context.FlushCommandQueue());
        SPIELER_RETURN_IF_FAILED(context.ResetCommandList());
        {
            
            // Reset Buffers and DepthStencil
            {
                for (Texture2D& buffer : m_Buffers)
                {
                    buffer.Reset();
                }

                m_DepthStencil.Reset();
            }

            SPIELER_RETURN_IF_FAILED(m_SwapChain->ResizeBuffers(
                m_BufferCount,
                width,
                height,
                static_cast<DXGI_FORMAT>(m_BufferFormat),
                DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
            ));

            SPIELER_RETURN_IF_FAILED(CreateBuffers(device, width, height));
            SPIELER_RETURN_IF_FAILED(CreateDepthStencil(device, width, height));
        }
        SPIELER_RETURN_IF_FAILED(context.ExecuteCommandList(true));

        return true;
    }

    bool SwapChain::Present(VSyncState vsync)
    {
        SPIELER_RETURN_IF_FAILED(m_SwapChain->Present(static_cast<UINT>(vsync), 0));

        return true;
    }

    bool SwapChain::InitFactory()
    {
#if defined(SPIELER_DEBUG)
        {
            ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
            SPIELER_RETURN_IF_FAILED(DXGIGetDebugInterface1(0, __uuidof(IDXGIInfoQueue), &dxgiInfoQueue));

            SPIELER_RETURN_IF_FAILED(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
            SPIELER_RETURN_IF_FAILED(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
        }
#endif

        SPIELER_RETURN_IF_FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), &m_Factory));
        SPIELER_RETURN_IF_FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory), &m_Factory));

        return true;
    }

    bool SwapChain::InitSwapChain(Context& context, Window& window)
    {
        const DXGI_MODE_DESC bufferDesc
        {
            .Width = window.GetWidth(),
            .Height = window.GetHeight(),
            .RefreshRate = DXGI_RATIONAL{ 60, 1 },
            .Format = static_cast<DXGI_FORMAT>(m_BufferFormat),
            .ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
            .Scaling = DXGI_MODE_SCALING_UNSPECIFIED,
        };

        const DXGI_SAMPLE_DESC sampleDesc
        {
            .Count = 1,
            .Quality = 0
        };

        DXGI_SWAP_CHAIN_DESC swapChainDesc
        {
            .BufferDesc = bufferDesc,
            .SampleDesc = sampleDesc,
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = m_BufferCount,
            .OutputWindow = window.GetNativeHandle<::HWND>(),
            .Windowed = true,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        };

        SPIELER_ASSERT(SUCCEEDED(m_Factory->CreateSwapChain(context.GetNativeCommandQueue().Get(), &swapChainDesc, &m_SwapChain)));
        SPIELER_ASSERT(SUCCEEDED(m_SwapChain->QueryInterface(__uuidof(IDXGISwapChain3), &m_SwapChain3)));

        return true;
    }

    bool SwapChain::CreateBuffers(Device& device, uint32_t width, uint32_t height)
    {
        for (uint32_t i = 0; i < m_BufferCount; ++i)
        {
            SPIELER_RETURN_IF_FAILED(m_SwapChain->GetBuffer(i, __uuidof(ID3D12Resource), &m_Buffers[i].GetResource().GetResource()));

            m_Buffers[i].GetResource().SetDebugName(L"SwapChainBuffer_" + std::to_wstring(i));
            m_Buffers[i].SetView<RenderTargetView>(device);
        }

        return true;
    }

    bool SwapChain::CreateDepthStencil(Device& device, uint32_t width, uint32_t height)
    {
        const Texture2DConfig depthStencilConfig
        {
            .Width = width,
            .Height = height,
            .Format = m_DepthStencilFormat,
            .Flags = Texture2DFlags::DepthStencil
        };

        const DepthStencilClearValue depthStencilClearValue
        {
            .Depth = 1.0f,
            .Stencil = 0
        };

        SPIELER_RETURN_IF_FAILED(m_DepthStencil.GetResource().InitAsDepthStencil(device, depthStencilConfig, depthStencilClearValue));
        m_DepthStencil.GetResource().SetDebugName(L"SwapChainDepthStencil");
        m_DepthStencil.SetView<DepthStencilView>(device);

        return true;
    }

} // namespace spieler::renderer