#include "application.h"

namespace Spieler
{

#define ERROR_CHECK(result)         \
{                                   \
    if (!result)                    \
    {                               \
        return false;               \
    }                               \
}

    LRESULT EventCallback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        auto& application = Application::GetInstance();

        switch (msg)
        {
            case WM_ACTIVATE:
            {
                if (LOWORD(wparam) == WA_INACTIVE)
                {
                    application.m_IsPaused = true;
                    application.m_Timer.Stop();
                }
                else
                {
                    application.m_IsPaused = false;
                    application.m_Timer.Start();
                }

                return 0;
            }

            case WM_ENTERSIZEMOVE:
            {
                application.m_IsPaused      = true;
                application.m_IsResizing    = true;

                application.m_Timer.Stop();

                return 0;
            }

            case WM_EXITSIZEMOVE:
            {
                application.m_IsPaused      = false;
                application.m_IsResizing    = false;
                
                application.m_Timer.Start();

                application.OnResize();

                return 0;
            }

            case WM_CLOSE:
            {
                DestroyWindow(hwnd);
                return 0;
            }

            case WM_DESTROY:
            {
                // PostQuitMessage(0);
                return 0;
            }

            case WM_GETMINMAXINFO:
            {
                reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.x = 200;
                reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.y = 200;
                return 0;
            }

            default:
            {
                break;
            }
        }

        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }

    bool Application::Init(const std::string& title, std::uint32_t width, std::uint32_t height)
    {
#if defined(SPIELER_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

        if (!m_WindowsRegisterClass.Init(EventCallback))
        {
            return false;
        }

        if (!m_Window.Init(m_WindowsRegisterClass, title, width, height))
        {
            return false;
        }

        if (!m_Window1.Init(m_WindowsRegisterClass, title + "Fuck", width, height))
        {
            return false;
        }

        ERROR_CHECK(InitDevice());
        ERROR_CHECK(InitFence());
        ERROR_CHECK(InitDescriptorSizes());
        ERROR_CHECK(InitMSAAQualitySupport());
        ERROR_CHECK(InitCommandQueue());
        ERROR_CHECK(InitCommandAllocator());
        ERROR_CHECK(InitCommandList());
        ERROR_CHECK(InitDXGIFactory());
        ERROR_CHECK(InitSwapChain());
        ERROR_CHECK(InitDescriptorHeaps());
        ERROR_CHECK(InitBackBufferRTV());
        ERROR_CHECK(InitDepthStencilTexture());
        ERROR_CHECK(InitViewport());
        ERROR_CHECK(InitScissor());

        return true;
    }

    void Application::Shutdown()
    {
        if (m_Device)
        {
            FlushCommandQueue();
        }

        m_WindowsRegisterClass.Shutdown();
    }

    int Application::Run()
    {
        MSG message{};

        m_Timer.Reset();

        while (message.message != WM_QUIT)
        {
            if (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&message);
                ::DispatchMessage(&message);
            }
            else
            {
                // TODO: Add m_IsPaused flag (if m_IsPaused -> ::Sleep(100))
                m_Timer.Tick();

                const float dt = m_Timer.GetDeltaTime();

                OnUpdate(dt);
                OnRender(dt);
            }
        }

        Shutdown();

        return static_cast<int>(message.lParam);
    }

    bool Application::InitDevice()
    {
#if defined(SPIELER_DEBUG)
        ComPtr<ID3D12Debug> debugController;

        const auto result = D3D12GetDebugInterface(__uuidof(ID3D12Debug), &debugController);

        if (FAILED(result))
        {
            return false;
        }

        debugController->EnableDebugLayer();
#endif
        const auto hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), &m_Device);

        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }

    bool Application::InitFence()
    {
        const auto result = m_Device->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_Fence);

        if (FAILED(result))
        {
            return false;
        }

        return true;
    }

    bool Application::InitDescriptorSizes()
    {
        m_RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_DSVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        m_CBVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        return true;
    }

    bool Application::InitMSAAQualitySupport()
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels{};
        qualityLevels.Format            = m_BackBufferFormat;
        qualityLevels.SampleCount       = 4;
        qualityLevels.Flags             = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        qualityLevels.NumQualityLevels  = 0;

        const auto result = m_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualityLevels, sizeof(qualityLevels));

        if (FAILED(result))
        {
            return false;
        }

        m_4xMSAAQuality = qualityLevels.NumQualityLevels;
        SPIELER_ASSERT(m_4xMSAAQuality > 0);

        return true;
    }

    bool Application::InitCommandQueue()
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type       = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority   = 0;
        desc.Flags      = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask   = 0;

        const auto result = m_Device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), &m_CommandQueue);

        if (FAILED(result))
        {
            return false;
        }

        return true;
    }

    bool Application::InitCommandAllocator()
    {
        const auto result = m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), &m_CommandAllocator);

        if (FAILED(result))
        {
            return false;
        }

        return true;
    }

    bool Application::InitCommandList()
    {
        const auto result = m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, __uuidof(ID3D12CommandList), &m_CommandList);

        if (FAILED(result))
        {
            return false;
        }

        m_CommandList->Close();

        return true;
    }

    bool Application::InitDXGIFactory()
    {
        const auto result = CreateDXGIFactory(__uuidof(IDXGIFactory), &m_DXGIFactory);

        if (FAILED(result))
        {
            return false;
        }

        return true;
    }

    bool Application::InitSwapChain()
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width                   = m_Window.GetWidth();
        desc.BufferDesc.Height                  = m_Window.GetHeight();
        desc.BufferDesc.RefreshRate.Numerator   = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferDesc.Format                  = m_BackBufferFormat;
        desc.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        desc.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
        desc.SampleDesc.Count                   = m_Use4xMSAA ? 4 : 1;
        desc.SampleDesc.Quality                 = m_Use4xMSAA ? (m_4xMSAAQuality - 1) : 0;
        desc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount                        = m_SwapChainBufferCount;
        desc.OutputWindow                       = m_Window.GetHandle();
        desc.Windowed                           = true;
        desc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        const auto result = m_DXGIFactory->CreateSwapChain(m_CommandQueue.Get(), &desc, &m_SwapChain);

        if (FAILED(result))
        {
            return false;
        }

        return true;
    }

    bool Application::InitRTVDescriptorHeap()
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = m_SwapChainBufferCount;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask       = 0;

        const auto result = m_Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_RTVDescriptorHeap);

        if (FAILED(result))
        {
            return false;
        }

        return true;
    }

    bool Application::InitDSVDescriptorHeap()
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = 1;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask       = 0;

        const auto result = m_Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_DSVDescriptorHeap);

        if (FAILED(result))
        {
            return false;
        }

        return true;
    }

    bool Application::InitBackBufferRTV()
    {
        m_SwapChainBuffers.resize(m_SwapChainBufferCount);

        auto heapHandle = m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

        for (std::size_t bufferIndex = 0; bufferIndex < m_SwapChainBuffers.size(); ++bufferIndex)
        {
            const auto result = m_SwapChain->GetBuffer(static_cast<UINT>(bufferIndex), __uuidof(ID3D12Resource), &m_SwapChainBuffers[bufferIndex]);

            if (FAILED(result))
            {
                return false;
            }

            m_Device->CreateRenderTargetView(
                m_SwapChainBuffers[bufferIndex].Get(), 
                nullptr, 
                D3D12_CPU_DESCRIPTOR_HANDLE{ m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + bufferIndex * m_RTVDescriptorSize }
            );
        }

        return true;
    }

    bool Application::InitDepthStencilTexture()
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment          = 0;
        desc.Width              = m_Window.GetWidth();
        desc.Height             = m_Window.GetHeight();
        desc.DepthOrArraySize   = 1;
        desc.MipLevels          = 1;
        desc.Format             = m_DepthStencilFormat;
        desc.SampleDesc.Count   = m_Use4xMSAA ? 4 : 1;
        desc.SampleDesc.Quality = m_Use4xMSAA ? m_4xMSAAQuality - 1 : 0;
        desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValueDesc{};
        clearValueDesc.Format               = m_DepthStencilFormat;
        clearValueDesc.DepthStencil.Depth   = 1.0f;
        clearValueDesc.DepthStencil.Stencil = 0;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type                  = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask      = 1;
        heapProps.VisibleNodeMask       = 1;

        {
            const auto result = m_Device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COMMON,
                &clearValueDesc,
                __uuidof(ID3D12Resource),
                &m_DepthStencilBuffer
            );

            if (FAILED(result))
            {
                return false;
            }
        }

        m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr, m_DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource    = m_DepthStencilBuffer.Get();
        barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        m_CommandList->ResourceBarrier(1, &barrier);

        return true;
    }

    bool Application::InitViewport()
    {
        m_ScreenViewport.TopLeftX   = 0.0f;
        m_ScreenViewport.TopLeftY   = 0.0f;
        m_ScreenViewport.Width      = static_cast<float>(m_Window.GetWidth());
        m_ScreenViewport.Height     = static_cast<float>(m_Window.GetHeight());
        m_ScreenViewport.MinDepth   = 0.0f;
        m_ScreenViewport.MaxDepth   = 1.0f;

        return true;
    }

    bool Application::InitScissor()
    {
        m_ScissorRect.left      = 0;
        m_ScissorRect.top       = 0;
        m_ScissorRect.right     = m_Window.GetWidth() / 2;
        m_ScissorRect.bottom    = m_Window.GetHeight() / 2;

        return true;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Application::GetCurrentBackBufferRTV()
    {
        return { m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + m_CurrentBackBuffer * m_RTVDescriptorSize };
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Application::GetDepthStencilView()
    {
        return m_DSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    }

    bool Application::InitDescriptorHeaps()
    {
        return InitRTVDescriptorHeap() && InitDSVDescriptorHeap();
    }

    void Application::FlushCommandQueue()
    {
        m_FenceValue++;

        m_CommandQueue->Signal(m_Fence.Get(), m_FenceValue);

        if (m_Fence->GetCompletedValue() < m_FenceValue)
        {
            HANDLE eventHandle = ::CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
            
            m_Fence->SetEventOnCompletion(m_FenceValue, eventHandle);

            ::WaitForSingleObject(eventHandle, INFINITE);
            ::CloseHandle(eventHandle);
        }
    }

    void Application::OnUpdate(float dt)
    {
        CalcStats();
    }

    void Application::OnRender(float dt)
    {
        m_CommandAllocator->Reset();
        m_CommandList->Reset(m_CommandAllocator.Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource    = m_SwapChainBuffers[m_CurrentBackBuffer].Get();
        barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        m_CommandList->ResourceBarrier(1, &barrier);

        m_CommandList->RSSetViewports(1, &m_ScreenViewport);
        m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

        const float clearColor[4] = { 0.5f, 0.3f, 0.7f, 1.0f };
        m_CommandList->ClearRenderTargetView(GetCurrentBackBufferRTV(), clearColor, 0, nullptr);

        m_CommandList->ClearDepthStencilView(GetDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

        auto a = GetCurrentBackBufferRTV();
        auto b = GetDepthStencilView();

        m_CommandList->OMSetRenderTargets(1, &a, true, &b);

        barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource    = m_SwapChainBuffers[m_CurrentBackBuffer].Get();
        barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        m_CommandList->ResourceBarrier(1, &barrier);

        m_CommandList->Close();

        ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
        m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

        m_CurrentBackBuffer = (m_CurrentBackBuffer + 1) % m_SwapChainBufferCount;

        {
            const auto result = m_SwapChain->Present(0, 0);

            if (FAILED(result))
            {
                return;
            }
        }

        FlushCommandQueue();
    }

    void Application::OnResize()
    {

    }

    void Application::CalcStats()
    {
        static std::uint32_t    fps         = 0;
        static float            timeElapsed = 0.0f;

        fps++;

        if (float delta = m_Timer.GetTotalTime() - timeElapsed; delta >= 1.0f)
        {
            m_Window.SetTitle("FPS: " + std::to_string(fps) + " ms: " + std::to_string(1000.0f / fps));

            fps          = 0;
            timeElapsed += delta;
        }
    }

} // namespace Spieler