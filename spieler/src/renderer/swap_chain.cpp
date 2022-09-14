#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/swap_chain.hpp"

#include "spieler/core/common.hpp"
#include "spieler/core/logger.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/context.hpp"

#include "platform/dx12/dx12_common.hpp"

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
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        }
#endif
    }

    Texture& SwapChain::GetCurrentBuffer()
    {
        return m_Buffers[GetCurrentBufferIndex()];
    }

    const Texture& SwapChain::GetCurrentBuffer() const
    {
        return m_Buffers[GetCurrentBufferIndex()];
    }

    uint32_t SwapChain::GetCurrentBufferIndex() const
    {
        return m_DXGISwapChain3->GetCurrentBackBufferIndex();
    }

    void SwapChain::ResizeBuffers(Device& device, uint32_t width, uint32_t height)
    {
        SPIELER_ASSERT(!m_Buffers.empty());

        for (Texture& buffer : m_Buffers)
        {
            buffer.Resource.Release();
            buffer.Views.Clear();
        }

        SPIELER_ASSERT(SUCCEEDED(m_DXGISwapChain->ResizeBuffers(
            static_cast<UINT>(m_Buffers.size()),
            width,
            height,
            dx12::Convert(m_BufferFormat),
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        )));

        CreateBuffers(device);
    }

    void SwapChain::Present(VSyncState vsync)
    {
        SPIELER_ASSERT(SUCCEEDED(m_DXGISwapChain->Present(static_cast<UINT>(vsync), 0)));
    }

    bool SwapChain::Init(Device& device, Context& context, Window& window, const SwapChainConfig& config)
    {
        m_Buffers.resize(config.BufferCount);

        SPIELER_RETURN_IF_FAILED(InitFactory());
        SPIELER_RETURN_IF_FAILED(InitSwapChain(context, window));
        
        EnumerateAdapters();
        ResizeBuffers(device, window.GetWidth(), window.GetHeight());

        return true;
    }

    bool SwapChain::InitFactory()
    {
#if defined(SPIELER_DEBUG)
        {
            ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
            SPIELER_RETURN_IF_FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)));

            SPIELER_RETURN_IF_FAILED(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
            SPIELER_RETURN_IF_FAILED(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
        }
#endif

        SPIELER_RETURN_IF_FAILED(CreateDXGIFactory(IID_PPV_ARGS(&m_DXGIFactory)));
        SPIELER_RETURN_IF_FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_DXGIFactory)));

        return true;
    }

    bool SwapChain::InitSwapChain(Context& context, Window& window)
    {
        SPIELER_ASSERT(!m_Buffers.empty());

        DXGI_SWAP_CHAIN_DESC swapChainDesc
        {
            .BufferDesc
            {
                .Width{ window.GetWidth() },
                .Height{ window.GetHeight() },
                .RefreshRate{ 60, 1 },
                .Format{ dx12::Convert(m_BufferFormat) },
                .ScanlineOrdering{ DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED },
                .Scaling{ DXGI_MODE_SCALING_UNSPECIFIED },
            },
            .SampleDesc
            {
                .Count{ 1 },
                .Quality{ 0 }
            },
            .BufferUsage{ DXGI_USAGE_RENDER_TARGET_OUTPUT },
            .BufferCount{ static_cast<UINT>(m_Buffers.size()) },
            .OutputWindow{ window.GetNativeHandle<::HWND>() },
            .Windowed{ true },
            .SwapEffect{ DXGI_SWAP_EFFECT_FLIP_DISCARD },
            .Flags{ DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH }
        };

        SPIELER_RETURN_IF_FAILED(m_DXGIFactory->CreateSwapChain(context.GetDX12CommandQueue(), &swapChainDesc, &m_DXGISwapChain));
        SPIELER_RETURN_IF_FAILED(m_DXGISwapChain->QueryInterface(IID_PPV_ARGS(&m_DXGISwapChain3)));

        return true;
    }

    void SwapChain::EnumerateAdapters()
    {
        ComPtr<IDXGIAdapter> dxgiAdapter;

        for (uint32_t adapterIndex = 0; m_DXGIFactory->EnumAdapters(adapterIndex, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
        {
            std::stringstream adapterStringStream;

            DXGI_ADAPTER_DESC dxgiAdapterDesc;
            dxgiAdapter->GetDesc(&dxgiAdapterDesc);

            const size_t adapterDescriptionSize{ std::size(dxgiAdapterDesc.Description) };

            std::string adapterName;
            adapterName.resize(adapterDescriptionSize);
            wcstombs_s(nullptr, adapterName.data(), adapterDescriptionSize, dxgiAdapterDesc.Description, adapterDescriptionSize * sizeof(wchar_t));

            adapterStringStream << fmt::format(
                "{}: {}\n"
                "       VendorID: {}\n"
                "       DeviceID: {}\n"
                "       DedicatedVideoMemory: {}MB, {}GB\n"
                "       DedicatedSystemMemory: {}MB, {}GB\n"
                "       SharedSystemMemory: {}MB, {}GB\n",
                adapterIndex, adapterName.c_str(),
                dxgiAdapterDesc.VendorId,
                dxgiAdapterDesc.DeviceId,
                ConvertBytesToMB(dxgiAdapterDesc.DedicatedVideoMemory), ConvertBytesToGB(dxgiAdapterDesc.DedicatedVideoMemory),
                ConvertBytesToMB(dxgiAdapterDesc.DedicatedSystemMemory), ConvertBytesToGB(dxgiAdapterDesc.DedicatedSystemMemory),
                ConvertBytesToMB(dxgiAdapterDesc.SharedSystemMemory), ConvertBytesToGB(dxgiAdapterDesc.SharedSystemMemory)
            );

            ComPtr<IDXGIOutput> dxgiOutput;

            for (uint32_t outputIndex = 0; dxgiAdapter->EnumOutputs(outputIndex, &dxgiOutput) != DXGI_ERROR_NOT_FOUND; ++outputIndex)
            {
                DXGI_OUTPUT_DESC dxgiOutputDesc;
                dxgiOutput->GetDesc(&dxgiOutputDesc);

                const size_t outputDescriptionSize{ std::size(dxgiOutputDesc.DeviceName) };

                std::string outputName;
                adapterName.resize(outputDescriptionSize);
                wcstombs_s(nullptr, outputName.data(), outputDescriptionSize, dxgiOutputDesc.DeviceName, outputDescriptionSize * sizeof(wchar_t));

                adapterStringStream << fmt::format("           {}: {}", outputIndex, outputName.c_str()); 
            }

            adapterStringStream << std::endl;

            SPIELER_INFO("{}", adapterStringStream.str().c_str());

            m_Adapters.push_back(std::move(dxgiAdapter));
        }
    }

    void SwapChain::CreateBuffers(Device& device)
    {
        for (size_t i = 0; i < m_Buffers.size(); ++i)
        {
            ComPtr<ID3D12Resource> backBuffer;
            SPIELER_ASSERT(SUCCEEDED(m_DXGISwapChain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&backBuffer))));
            
            m_Buffers[i].Resource = TextureResource{ std::move(backBuffer) };

            m_Buffers[i].Views.Clear();
            m_Buffers[i].Views.CreateView<RenderTargetView>(device);
        }
    }

} // namespace spieler::renderer
