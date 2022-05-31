#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/renderer.hpp"

#include "spieler/system/window.hpp"
#include "spieler/renderer/pipeline_state.hpp"

namespace spieler::renderer
{

    static Renderer* g_Instance{ nullptr };

    constexpr SwapChainConfig g_SwapChainConfig
    {
        .BufferCount = 2,
        .BufferFormat = GraphicsFormat::R8G8B8A8_UNORM,
        .DepthStencilFormat = GraphicsFormat::D24_UNORM_S8_UINT
    };

    Renderer& Renderer::CreateInstance(Window& window)
    {
        SPIELER_ASSERT(!g_Instance);

        g_Instance = new Renderer(window);

        return *g_Instance;
    }

    Renderer& Renderer::GetInstance()
    {
        SPIELER_ASSERT(g_Instance);

        return *g_Instance;
    }

    void Renderer::DestoryInstance()
    {
        delete g_Instance;
        g_Instance = nullptr;
    }

    Renderer::Renderer(Window& window)
        : m_Context(m_Device)
        , m_SwapChain(m_Device, m_Context, window, g_SwapChainConfig)
    {}

    Renderer::~Renderer()
    {
        if (m_Device.GetNativeDevice())
        {
            m_Context.FlushCommandQueue();
        }
    }

    void Renderer::SetDefaultRenderTargets()
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE backBufferCPUDescriptorHandle{ m_SwapChain.GetCurrentBuffer().GetView<RenderTargetView>().GetDescriptor().CPU };
        const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilCPUDescriptorHandle{ m_SwapChain.GetDepthStencil().GetView<DepthStencilView>().GetDescriptor().CPU };

        m_Context.GetNativeCommandList()->OMSetRenderTargets(
            1,
            &backBufferCPUDescriptorHandle,
            true,
            &depthStencilCPUDescriptorHandle
        );
    }

    void Renderer::ClearRenderTarget(const DirectX::XMFLOAT4& color)
    {
        m_Context.GetNativeCommandList()->ClearRenderTargetView(
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_SwapChain.GetCurrentBuffer().GetView<RenderTargetView>().GetDescriptor().CPU },
            reinterpret_cast<const float*>(&color),
            0,
            nullptr
        );
    }

    void Renderer::ClearDepth(float depth)
    {
        ClearDepthStencil(depth, 0, D3D12_CLEAR_FLAG_DEPTH);
    }

    void Renderer::ClearStencil(uint8_t stencil)
    {
        ClearDepthStencil(0.0f, stencil, D3D12_CLEAR_FLAG_STENCIL);
    }

    void Renderer::ClearDepthStencil(float depth, uint8_t stencil)
    {
        ClearDepthStencil(depth, stencil, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
    }

    bool Renderer::ResizeBuffers(uint32_t width, uint32_t height)
    {
        return m_SwapChain.ResizeBuffers(m_Device, m_Context, width, height);
    }

    bool Renderer::Present(renderer::VSyncState vsync)
    {
        return m_SwapChain.Present(vsync);
    }

    void Renderer::ClearDepthStencil(float depth, uint8_t stencil, D3D12_CLEAR_FLAGS flags)
    {
        m_Context.GetNativeCommandList()->ClearDepthStencilView(
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_SwapChain.GetDepthStencil().GetView<DepthStencilView>().GetDescriptor().CPU },
            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 
            depth, 
            stencil, 
            0, 
            nullptr
        );
    }

} // namespace spieler::renderer