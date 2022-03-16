#pragma once

#include <vector>

#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

#include "descriptor_heap.h"
#include "fence.h"
#include "pipeline_state.h"

namespace Spieler
{

    class Window;
    class DescriptorHeap;

    enum VSyncState : bool
    {
        VSyncState_Disable  = false,
        VSyncState_Enable   = true
    };

    struct SwapChainProps
    {
        DXGI_FORMAT     BufferFormat{ DXGI_FORMAT_UNKNOWN };
        std::uint32_t   BufferCount{ 0 };
    };

    struct Viewport
    {
        float X{ 0.0f };
        float Y{ 0.0f };
        float Width{ 0.0f };
        float Height{ 0.0f };
        float MinDepth{ 0.0f };
        float MaxDepth{ 1.0f };
    };

    struct ScissorRect
    {
        float X{ 0.0f };
        float Y{ 0.0f };
        float Width{ 0.0f };
        float Height{ 0.0f };
    };

    class Renderer
    {
    public:
        friend class Application;
        friend class RendererObject;

    public:
        ~Renderer();

    public:
        const SwapChainProps& GetSwapChainProps() const { return m_SwapChainProps; }
       
        ComPtr<ID3D12Resource> GetSwapChainBuffer(std::uint32_t index) { return m_SwapChainBuffers[index]; }

        DXGI_FORMAT GetDepthStencilFormat() const { return m_DepthStencilFormat; }

    public:
        bool Init(const Window& window);

        void SetViewport(const Viewport& viewport);
        void SetScissorRect(const ScissorRect& rect);

        void ClearRenderTarget(const DirectX::XMFLOAT4& color);
        void ClearDepth(float depth);
        void ClearStencil(std::uint8_t stencil);
        void ClearDepthStencil(float depth, std::uint8_t stencil);

        bool ResizeBuffers(std::uint32_t width, std::uint32_t height);

        bool ResetCommandList(PipelineState pso = {});
        bool ExexuteCommandList(bool isNeedToFlushCommandQueue = true);
        bool FlushCommandQueue();

        bool SwapBuffers(VSyncState vsync);

    private:
        bool InitDevice();
        bool InitCommandQueue();
        bool InitCommandAllocator();
        bool InitCommandList();
        bool InitFactory();
        bool InitSwapChain(const Window& window);
        bool InitDescriptorHeaps();
        bool InitFence();

        void ClearDepthStencil(float depth, std::uint8_t stencil, D3D12_CLEAR_FLAGS flags);

    private:
        const SwapChainProps    m_SwapChainProps{ DXGI_FORMAT_R8G8B8A8_UNORM, 2 };
        const DXGI_FORMAT       m_DepthStencilFormat{ DXGI_FORMAT_D24_UNORM_S8_UINT };

    public:
        ComPtr<ID3D12Device>                m_Device;
        ComPtr<IDXGIFactory>                m_Factory;
        ComPtr<IDXGISwapChain>              m_SwapChain;
        ComPtr<IDXGISwapChain3>             m_SwapChain3;

        ComPtr<ID3D12CommandAllocator>      m_CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList>   m_CommandList;
        ComPtr<ID3D12CommandQueue>          m_CommandQueue;

        Fence                               m_Fence;

        std::vector<ComPtr<ID3D12Resource>> m_SwapChainBuffers;
        ComPtr<ID3D12Resource>              m_DepthStencilBuffer;

        DescriptorHeap                      m_SwapChainBufferRTVDescriptorHeap;
        DescriptorHeap                      m_DSVDescriptorHeap;
    };

} // namespace Spieler
