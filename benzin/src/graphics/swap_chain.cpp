#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/swap_chain.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "benzin/core/common.hpp"

#include "benzin/system/window.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/resource_view_builder.hpp"

namespace benzin
{

    static size_t g_DefaultBackBufferCount = 2;

    SwapChain::SwapChain(const Window& window, Device& device, CommandQueue& commandQueue, const char* debugName)
    {
        m_BackBuffers.resize(g_DefaultBackBufferCount);

        // DXGI Factory
        {
            BENZIN_D3D12_ASSERT(CreateDXGIFactory1(IID_PPV_ARGS(&m_DXGIFactory4)));
        }

        // D3D12 SwapChain
        {
            const DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc1
            {
                .Width{ window.GetWidth() },
                .Height{ window.GetHeight() },
                .Format{ static_cast<DXGI_FORMAT>(m_BackBufferFormat) },
                .Stereo{ false },
                .SampleDesc
                {
                    .Count{ 1 },
                    .Quality{ 0 }
                },
                .BufferUsage{ DXGI_USAGE_RENDER_TARGET_OUTPUT },
                .BufferCount{ static_cast<UINT>(m_BackBuffers.size()) },
                .Scaling{ DXGI_SCALING_STRETCH },
                .SwapEffect{ DXGI_SWAP_EFFECT_FLIP_DISCARD },
                .AlphaMode{ DXGI_ALPHA_MODE_UNSPECIFIED },
                .Flags{ DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT }
            };

            ComPtr<IDXGISwapChain1> dxgiSwapChain1;

            m_DXGIFactory4->CreateSwapChainForHwnd(
                commandQueue.GetD3D12CommandQueue(),
                window.GetWin64Window(),
                &dxgiSwapChainDesc1,
                nullptr,
                nullptr,
                &dxgiSwapChain1
            );

            BENZIN_D3D12_ASSERT(dxgiSwapChain1->QueryInterface(IID_PPV_ARGS(&m_DXGISwapChain3)));

            m_DXGISwapChain3->SetMaximumFrameLatency(2);
            
            SetDebugName(debugName);
        }

        EnumerateAdapters();
        ResizeBackBuffers(device, window.GetWidth(), window.GetHeight());
    }

    SwapChain::~SwapChain()
    {
        SafeReleaseD3D12Object(m_DXGISwapChain3);
        SafeReleaseD3D12Object(m_DXGIFactory4);
    }

    std::shared_ptr<TextureResource>& SwapChain::GetCurrentBackBuffer()
    {
        return m_BackBuffers[GetCurrentBufferIndex()];
    }

    const std::shared_ptr<TextureResource>& SwapChain::GetCurrentBackBuffer() const
    {
        return m_BackBuffers[GetCurrentBufferIndex()];
    }

    uint32_t SwapChain::GetCurrentBufferIndex() const
    {
        return m_DXGISwapChain3->GetCurrentBackBufferIndex();
    }

    void SwapChain::ResizeBackBuffers(Device& device, uint32_t width, uint32_t height)
    {
        BENZIN_ASSERT(!m_BackBuffers.empty());

        for (std::shared_ptr<TextureResource>& backBuffer : m_BackBuffers)
        {
            backBuffer.reset();
        }

        BENZIN_D3D12_ASSERT(m_DXGISwapChain3->ResizeBuffers(
            static_cast<UINT>(m_BackBuffers.size()),
            width,
            height,
            static_cast<DXGI_FORMAT>(m_BackBufferFormat),
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
        ));

        CreateBackBuffers(device);
    }

    void SwapChain::Flip(VSyncState vsync)
    {
        BENZIN_D3D12_ASSERT(m_DXGISwapChain3->Present(static_cast<UINT>(vsync), 0));
    }

    void SwapChain::EnumerateAdapters()
    {
        ComPtr<IDXGIAdapter> dxgiAdapter;

        for (uint32_t adapterIndex = 0; m_DXGIFactory4->EnumAdapters(adapterIndex, &dxgiAdapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
        {
            std::stringstream adapterStringStream;

            DXGI_ADAPTER_DESC dxgiAdapterDesc;
            dxgiAdapter->GetDesc(&dxgiAdapterDesc);

            const size_t adapterDescriptionSize = std::size(dxgiAdapterDesc.Description);

            std::string adapterName;
            adapterName.resize(adapterDescriptionSize);
            wcstombs_s(nullptr, adapterName.data(), adapterDescriptionSize, dxgiAdapterDesc.Description, adapterDescriptionSize * sizeof(wchar_t));

            adapterStringStream << fmt::format(
                "\n"
                "  {}: {}\n"
                "  VendorID: {}\n"
                "  DeviceID: {}\n"
                "  DedicatedVideoMemory: {}MB, {}GB\n"
                "  DedicatedSystemMemory: {}MB, {}GB\n"
                "  SharedSystemMemory: {}MB, {}GB\n",
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

                const size_t outputDescriptionSize = std::size(dxgiOutputDesc.DeviceName);

                std::string outputName;
                adapterName.resize(outputDescriptionSize);
                wcstombs_s(nullptr, outputName.data(), outputDescriptionSize, dxgiOutputDesc.DeviceName, outputDescriptionSize * sizeof(wchar_t));

                adapterStringStream << fmt::format("    {}: {}\n", outputIndex, outputName.c_str()); 
            }

            BENZIN_INFO("{}", adapterStringStream.str().c_str());
        }
    }

    void SwapChain::CreateBackBuffers(Device& device)
    {
        for (size_t i = 0; i < m_BackBuffers.size(); ++i)
        {
            ID3D12Resource* d3d12BackBuffer;
            BENZIN_D3D12_ASSERT(m_DXGISwapChain3->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&d3d12BackBuffer)));
            
            auto& backBuffer = m_BackBuffers[i];
            backBuffer = device.RegisterTextureResource(d3d12BackBuffer, "SwapChainBackBuffer" + std::to_string(i));
            backBuffer->PushRenderTargetView(device.GetResourceViewBuilder().CreateRenderTargetView(*backBuffer));
        }
    }

} // namespace benzin
