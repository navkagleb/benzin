#include "renderer.h"

#include "window.h"

namespace Spieler
{

    bool Renderer::Init(const Window& window)
    {
        SPIELER_CHECK_STATUS(InitDevice());
        SPIELER_CHECK_STATUS(InitCommandAllocator());
        SPIELER_CHECK_STATUS(InitCommandList());
        SPIELER_CHECK_STATUS(InitCommandQueue());
        SPIELER_CHECK_STATUS(InitFactory());
        SPIELER_CHECK_STATUS(InitSwapChain(window));
        SPIELER_CHECK_STATUS(InitDescriptorHeaps());

        return true;
    }

    bool Renderer::InitDevice()
    {
#if defined(SPIELER_DEBUG)
        ComPtr<ID3D12Debug> debugController;

        SPIELER_CHECK_STATUS(D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugController));

        debugController->EnableDebugLayer();
#endif
        SPIELER_CHECK_STATUS(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), &m_Device));

        return true;
    }

    bool Renderer::InitCommandAllocator()
    {
        SPIELER_CHECK_STATUS(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &m_CommandAllocator));

        return true;
    }

    bool Renderer::InitCommandList()
    {
        SPIELER_CHECK_STATUS(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, __uuidof(ID3D12CommandList), &m_CommandList));

        return true;
    }

    bool Renderer::InitCommandQueue()
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type       = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority   = 0;
        desc.Flags      = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask   = 0;

        SPIELER_CHECK_STATUS(m_Device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), &m_CommandQueue));

        return true;
    }

    bool Renderer::InitFactory()
    {
        SPIELER_CHECK_STATUS(CreateDXGIFactory(__uuidof(IDXGIFactory), &m_Factory));

        return true;
    }

    bool Renderer::InitSwapChain(const Window& window)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width                   = window.GetWidth();
        desc.BufferDesc.Height                  = window.GetHeight();
        desc.BufferDesc.RefreshRate.Numerator   = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferDesc.Format                  = m_BackBufferFormat;
        desc.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        desc.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
#if 0
        desc.SampleDesc.Count                   = m_Use4xMSAA ? 4 : 1;
        desc.SampleDesc.Quality                 = m_Use4xMSAA ? (m_4xMSAAQuality - 1) : 0;
#else
        desc.SampleDesc.Count                   = 1;
        desc.SampleDesc.Quality                 = 0;
#endif
        desc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount                        = m_SwapChainBufferCount;
        desc.OutputWindow                       = window.GetHandle();
        desc.Windowed                           = true;
        desc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        SPIELER_CHECK_STATUS(m_Factory->CreateSwapChain(m_CommandQueue.Get(), &desc, &m_SwapChain));
        SPIELER_CHECK_STATUS(m_SwapChain->QueryInterface(__uuidof(IDXGISwapChain3), &m_SwapChain3));

        return true;
    }

    bool Renderer::InitDescriptorHeaps()
    {
        SPIELER_CHECK_STATUS(m_MixtureDescriptorHeap.Init(DescriptorHeapType_CBVSRVUAV, m_SwapChainBufferCount));
        SPIELER_CHECK_STATUS(m_RTVDescriptorHeap.Init(DescriptorHeapType_RTV, m_SwapChainBufferCount));
        SPIELER_CHECK_STATUS(m_DSVDescriptorHeap.Init(DescriptorHeapType_DSV, m_SwapChainBufferCount));

        return true;
    }

} // namespace Spieler