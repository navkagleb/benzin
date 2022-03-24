#include "renderer/renderer.hpp"

#include <d3dx12.h>
#include <dxgidebug.h>

#include "window/window.hpp"
#include "renderer/pipeline_state.hpp"

namespace Spieler
{

    Renderer::~Renderer()
    {
        if (m_Device)
        {
            FlushCommandQueue();
        }

#if defined(SPIELER_DEBUG)
        {
            ComPtr<IDXGIDebug1> dxgiDebug;

            if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
            {
                dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
            }
        }
#endif
    }

    bool Renderer::Init(const Window& window)
    {
        SPIELER_RETURN_IF_FAILED(InitDevice());
        SPIELER_RETURN_IF_FAILED(InitCommandAllocator());
        SPIELER_RETURN_IF_FAILED(InitCommandList());
        SPIELER_RETURN_IF_FAILED(InitCommandQueue());
        SPIELER_RETURN_IF_FAILED(InitFactory());
        SPIELER_RETURN_IF_FAILED(InitSwapChain(window));
        SPIELER_RETURN_IF_FAILED(InitDescriptorHeaps());
        SPIELER_RETURN_IF_FAILED(InitFence());

        SPIELER_RETURN_IF_FAILED(ResizeBuffers(window.GetWidth(), window.GetHeight()));

        return true;
    }

    void Renderer::SetViewport(const Viewport& viewport)
    {
        m_CommandList->RSSetViewports(1, reinterpret_cast<const D3D12_VIEWPORT*>(&viewport));
    }

    void Renderer::SetScissorRect(const ScissorRect& rect)
    {
        RECT d3d12Rect{};
        d3d12Rect.left      = static_cast<LONG>(rect.X);
        d3d12Rect.top       = static_cast<LONG>(rect.Y);
        d3d12Rect.right     = static_cast<LONG>(rect.X + rect.Width);
        d3d12Rect.bottom    = static_cast<LONG>(rect.Y + rect.Height);

        m_CommandList->RSSetScissorRects(1, &d3d12Rect);
    }

    void Renderer::ClearRenderTarget(const DirectX::XMFLOAT4& color)
    {
        m_CommandList->ClearRenderTargetView(
            m_SwapChainBufferRTVDescriptorHeap.GetCPUHandle(m_SwapChain3->GetCurrentBackBufferIndex()),
            reinterpret_cast<const float*>(&color),
            0,
            nullptr
        );
    }

    void Renderer::ClearDepth(float depth)
    {
        ClearDepthStencil(depth, 0, D3D12_CLEAR_FLAG_DEPTH);
    }

    void Renderer::ClearStencil(std::uint8_t stencil)
    {
        ClearDepthStencil(0.0f, stencil, D3D12_CLEAR_FLAG_STENCIL);
    }

    void Renderer::ClearDepthStencil(float depth, std::uint8_t stencil)
    {
        ClearDepthStencil(depth, stencil, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
    }

    bool Renderer::ResizeBuffers(std::uint32_t width, std::uint32_t height)
    {
        SPIELER_ASSERT(m_Device);
        SPIELER_ASSERT(m_SwapChain);
        SPIELER_ASSERT(m_CommandAllocator);

        FlushCommandQueue();

        SPIELER_RETURN_IF_FAILED(m_CommandList->Reset(m_CommandAllocator.Get(), nullptr));

        if (m_SwapChainBuffers.empty())
        {
            m_SwapChainBuffers.resize(m_SwapChainProps.BufferCount);
        }
        else
        {
            for (std::uint32_t i = 0; i < m_SwapChainProps.BufferCount; ++i)
            {
                m_SwapChainBuffers[i].Reset();
            }

            m_DepthStencilBuffer.Reset();

            SPIELER_RETURN_IF_FAILED(m_SwapChain->ResizeBuffers(
                m_SwapChainProps.BufferCount,
                width,
                height,
                m_SwapChainProps.BufferFormat,
                DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
            ));
        }

        for (std::uint32_t i = 0; i < m_SwapChainProps.BufferCount; ++i)
        {
            SPIELER_RETURN_IF_FAILED(m_SwapChain->GetBuffer(i, __uuidof(ID3D12Resource), &m_SwapChainBuffers[i]));

            m_Device->CreateRenderTargetView(m_SwapChainBuffers[i].Get(), nullptr, m_SwapChainBufferRTVDescriptorHeap.GetCPUHandle(i));
        }

        D3D12_RESOURCE_DESC depthStencilDesc{};
        depthStencilDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment          = 0;
        depthStencilDesc.Width              = width;
        depthStencilDesc.Height             = height;
        depthStencilDesc.DepthOrArraySize   = 1;
        depthStencilDesc.MipLevels          = 1; 
        depthStencilDesc.Format             = DXGI_FORMAT_R24G8_TYPELESS;
        depthStencilDesc.SampleDesc.Count   = 1;
        depthStencilDesc.SampleDesc.Quality = 0;
        depthStencilDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValueDesc{};
        clearValueDesc.Format               = m_DepthStencilFormat;
        clearValueDesc.DepthStencil.Depth   = 1.0f;
        clearValueDesc.DepthStencil.Stencil = 0;

        const D3D12_HEAP_PROPERTIES defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        SPIELER_RETURN_IF_FAILED(m_Device->CreateCommittedResource(
            &defaultHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &clearValueDesc,
            __uuidof(ID3D12Resource),
            &m_DepthStencilBuffer
        ));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Flags               = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension       = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format              = m_DepthStencilFormat;
        dsvDesc.Texture2D.MipSlice  = 0;

        m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), &dsvDesc, m_DSVDescriptorHeap.GetCPUHandle(0));

        const D3D12_RESOURCE_BARRIER resourceTransitionBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_DepthStencilBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON, 
            D3D12_RESOURCE_STATE_DEPTH_WRITE
        );

        m_CommandList->ResourceBarrier(1, &resourceTransitionBarrier);

        SPIELER_RETURN_IF_FAILED(m_CommandList->Close());

        m_CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(m_CommandList.GetAddressOf()));

        FlushCommandQueue();

        return true;
    }

    bool Renderer::ResetCommandList(PipelineState pso)
    {
        SPIELER_RETURN_IF_FAILED(m_CommandList->Reset(m_CommandAllocator.Get(), static_cast<ID3D12PipelineState*>(pso)));

        return true;
    }

    bool Renderer::ExexuteCommandList(bool isNeedToFlushCommandQueue)
    {
        SPIELER_RETURN_IF_FAILED(m_CommandList->Close());

        m_CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(m_CommandList.GetAddressOf()));

        if (isNeedToFlushCommandQueue)
        {
            FlushCommandQueue();
        }

        return true;
    }

    bool Renderer::FlushCommandQueue()
    {
        SPIELER_RETURN_IF_FAILED(m_CommandQueue->Signal(m_Fence.Handle.Get(), ++m_Fence.Value));

        if (m_Fence.Handle->GetCompletedValue() < m_Fence.Value)
        {
            const HANDLE eventHandle = ::CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);

            m_Fence.Handle->SetEventOnCompletion(m_Fence.Value, eventHandle);

            ::WaitForSingleObject(eventHandle, INFINITE);
            ::CloseHandle(eventHandle);
        }

        return true;
    }

    bool Renderer::SwapBuffers(VSyncState vsync)
    {
        SPIELER_RETURN_IF_FAILED(m_SwapChain->Present(static_cast<UINT>(vsync), 0));

        return true;
    }

    bool Renderer::InitDevice()
    {
#if defined(SPIELER_DEBUG)
        {
            ComPtr<ID3D12Debug> debugController;
            SPIELER_RETURN_IF_FAILED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugController));

            debugController->EnableDebugLayer();
        }
#endif

        SPIELER_RETURN_IF_FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), &m_Device));

        return true;
    }

    bool Renderer::InitCommandAllocator()
    {
        SPIELER_RETURN_IF_FAILED(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &m_CommandAllocator));

        return true;
    }

    bool Renderer::InitCommandList()
    {
        SPIELER_RETURN_IF_FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, __uuidof(ID3D12CommandList), &m_CommandList));
        SPIELER_RETURN_IF_FAILED(m_CommandList->Close());

        return true;
    }

    bool Renderer::InitCommandQueue()
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type       = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority   = 0;
        desc.Flags      = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask   = 0;

        SPIELER_RETURN_IF_FAILED(m_Device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), &m_CommandQueue));

        return true;
    }

    bool Renderer::InitFactory()
    {
#if defined(SPIELER_DEBUG)
        {
            ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
            SPIELER_RETURN_IF_FAILED(DXGIGetDebugInterface1(0, __uuidof(IDXGIInfoQueue), &dxgiInfoQueue));

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        }
#endif

        SPIELER_RETURN_IF_FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), &m_Factory));

        return true;
    }

    bool Renderer::InitSwapChain(const Window& window)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width                   = window.GetWidth();
        desc.BufferDesc.Height                  = window.GetHeight();
        desc.BufferDesc.RefreshRate.Numerator   = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferDesc.Format                  = m_SwapChainProps.BufferFormat;
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
        desc.BufferCount                        = m_SwapChainProps.BufferCount;
        desc.OutputWindow                       = window.GetHandle();
        desc.Windowed                           = true;
        desc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        SPIELER_RETURN_IF_FAILED(m_Factory->CreateSwapChain(m_CommandQueue.Get(), &desc, &m_SwapChain));
        SPIELER_RETURN_IF_FAILED(m_SwapChain->QueryInterface(__uuidof(IDXGISwapChain3), &m_SwapChain3));

        return true;
    }

    bool Renderer::InitDescriptorHeaps()
    {
        SPIELER_RETURN_IF_FAILED(m_SwapChainBufferRTVDescriptorHeap.Init(DescriptorHeapType_RTV, m_SwapChainProps.BufferCount));
        SPIELER_RETURN_IF_FAILED(m_DSVDescriptorHeap.Init(DescriptorHeapType_DSV, 1));

        return true;
    }

    bool Renderer::InitFence()
    {
        SPIELER_RETURN_IF_FAILED(m_Fence.Init());

        return true;
    }

    void Renderer::ClearDepthStencil(float depth, std::uint8_t stencil, D3D12_CLEAR_FLAGS flags)
    {
        m_CommandList->ClearDepthStencilView(m_DSVDescriptorHeap.GetCPUHandle(0), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depth, stencil, 0, nullptr);
    }

} // namespace Spieler