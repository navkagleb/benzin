#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/swap_chain.hpp"

#include "spieler/core/common.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/context.hpp"

namespace spieler::renderer
{

    SwapChain::SwapChain(Device& device, Context& context, Window& window, const SwapChainConfig& config)
        : m_BufferFormat{ config.BufferFormat }
    {
        SPIELER_ASSERT(Init(device, context, window, config));
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

    uint32_t SwapChain::GetCurrentBufferIndex() const
    {
        return m_SwapChain3->GetCurrentBackBufferIndex();
    }

    Texture2D& SwapChain::GetCurrentBuffer()
    {
        return m_Buffers[GetCurrentBufferIndex()];
    }

    const Texture2D& SwapChain::GetCurrentBuffer() const
    {
        return m_Buffers[GetCurrentBufferIndex()];
    }

    bool SwapChain::ResizeBuffers(Device& device, Context& context, uint32_t width, uint32_t height)
    {
        SPIELER_ASSERT(!m_Buffers.empty());

        SPIELER_RETURN_IF_FAILED(context.FlushCommandQueue());
        SPIELER_RETURN_IF_FAILED(context.ResetCommandList());
        {

            for (Texture2D& buffer : m_Buffers)
            {
                buffer.GetTexture2DResource().Reset();
            }

            SPIELER_RETURN_IF_FAILED(m_SwapChain->ResizeBuffers(
                static_cast<UINT>(m_Buffers.size()),
                width,
                height,
                static_cast<DXGI_FORMAT>(m_BufferFormat),
                DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
            ));

            SPIELER_RETURN_IF_FAILED(CreateBuffers(device));
        }
        SPIELER_RETURN_IF_FAILED(context.ExecuteCommandList(true));

        return true;
    }

    bool SwapChain::Present(VSyncState vsync)
    {
        SPIELER_RETURN_IF_FAILED(m_SwapChain->Present(static_cast<UINT>(vsync), 0));

        return true;
    }

    bool SwapChain::Init(Device& device, Context& context, Window& window, const SwapChainConfig& config)
    {
        m_Buffers.resize(config.BufferCount);

        SPIELER_RETURN_IF_FAILED(InitFactory());
        SPIELER_RETURN_IF_FAILED(InitSwapChain(context, window));
        SPIELER_RETURN_IF_FAILED(ResizeBuffers(device, context, window.GetWidth(), window.GetHeight()));

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
        SPIELER_ASSERT(!m_Buffers.empty());

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
            .BufferCount = static_cast<UINT>(m_Buffers.size()),
            .OutputWindow = window.GetNativeHandle<::HWND>(),
            .Windowed = true,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        };

        SPIELER_ASSERT(SUCCEEDED(m_Factory->CreateSwapChain(context.GetNativeCommandQueue().Get(), &swapChainDesc, &m_SwapChain)));
        SPIELER_ASSERT(SUCCEEDED(m_SwapChain->QueryInterface(__uuidof(IDXGISwapChain3), &m_SwapChain3)));

        return true;
    }

    bool SwapChain::CreateBuffers(Device& device)
    {
        for (size_t i = 0; i < m_Buffers.size(); ++i)
        {
            ComPtr<ID3D12Resource> buffer;
            SPIELER_RETURN_IF_FAILED(m_SwapChain->GetBuffer(static_cast<UINT>(i), __uuidof(ID3D12Resource), &buffer));
            
            device.RegisterTexture(std::move(buffer), m_Buffers[i].GetTexture2DResource());
            
            m_Buffers[i].GetTexture2DResource().SetDebugName(L"SwapChainBuffer_" + std::to_wstring(i));

            m_Buffers[i].SetView<RenderTargetView>(device);
        }

        return true;
    }

} // namespace spieler::renderer