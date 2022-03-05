#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>

#include "common.h"
#include "descriptor_heap.h"

namespace Spieler
{

    class Window;
    class DescriptorHeap;

    class Renderer
    {
    public:
        friend class Application;
        friend class RendererObject;

    public:
        bool Init(const Window& window);

    private:
        bool InitDevice();
        bool InitCommandQueue();
        bool InitCommandAllocator();
        bool InitCommandList();
        bool InitFactory();
        bool InitSwapChain(const Window& window);
        bool InitDescriptorHeaps();

    private:
        const std::uint32_t                 m_SwapChainBufferCount  = 2;
        const DXGI_FORMAT                   m_BackBufferFormat      = DXGI_FORMAT_R8G8B8A8_UNORM;

    private:
        ComPtr<ID3D12Device>                m_Device;
        ComPtr<IDXGIFactory>                m_Factory;
        ComPtr<IDXGISwapChain>              m_SwapChain;
        ComPtr<IDXGISwapChain3>             m_SwapChain3;

        ComPtr<ID3D12CommandAllocator>      m_CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList>   m_CommandList;
        ComPtr<ID3D12CommandQueue>          m_CommandQueue;

        DescriptorHeap                      m_MixtureDescriptorHeap;
        DescriptorHeap                      m_RTVDescriptorHeap;
        DescriptorHeap                      m_DSVDescriptorHeap;
    };

} // namespace Spieler
