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

    bool Application::Init(const std::string& title, std::uint32_t width, std::uint32_t height)
    {
        if (!m_WindowsRegisterClass.Init())
        {
            return false;
        }

        if (!m_Window.Init(m_WindowsRegisterClass, title, width, height))
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

        return true;
    }

    void Application::Shutdown()
    {
        m_WindowsRegisterClass.Shutdown();
    }

    int Application::Run()
    {
        MSG message{};

        while (true)
        {
            if (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    break;
                }

                ::TranslateMessage(&message);
                ::DispatchMessage(&message);
            }

            const auto result = m_SwapChain->Present(0, 0);

            if (FAILED(result))
            {
                return -1;
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
        const auto result = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &m_Fence);

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

        const auto result = m_Device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_RTVDescriptorHeap);

        if (FAILED(result))
        {
            return false;
        }

        return true;
    }

    bool Application::InitDescriptorHeaps()
    {
        return InitRTVDescriptorHeap() && InitDSVDescriptorHeap();
    }

    void Application::FlushCommandQueue()
    {

    }

} // namespace Spieler