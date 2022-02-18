#pragma once

#include <cstdint>

#include <d3d12.h>
#include <dxgi.h>

#include "common.h"
#include "window.h"

namespace Spieler
{

    class Application
    {
    public:
        bool Init(const std::string& title, std::uint32_t width, std::uint32_t height);
        void Shutdown();

        int Run();

    private:
        bool InitDevice();
        bool InitFence();
        bool InitDescriptorSizes();
        bool InitMSAAQualitySupport();
        bool InitCommandQueue();
        bool InitCommandAllocator();
        bool InitCommandList();
        bool InitDXGIFactory();
        bool InitSwapChain();
        bool InitRTVDescriptorHeap();
        bool InitDSVDescriptorHeap();
        bool InitDescriptorHeaps();

        void FlushCommandQueue();

    private:
        WindowsRegisterClass                m_WindowsRegisterClass;
        Window                              m_Window;

        ComPtr<ID3D12Device>                m_Device;
        ComPtr<ID3D12Fence>                 m_Fence;
        ComPtr<ID3D12CommandQueue>          m_CommandQueue;
        ComPtr<ID3D12CommandAllocator>      m_CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList>   m_CommandList;
        ComPtr<IDXGIFactory>                m_DXGIFactory;
        ComPtr<IDXGISwapChain>              m_SwapChain;
        ComPtr<ID3D12DescriptorHeap>        m_RTVDescriptorHeap;
        ComPtr<ID3D12DescriptorHeap>        m_DSVDescriptorHeap;

        std::uint32_t                       m_CurrentBackBuffer     = 0;

        std::uint32_t                       m_RTVDescriptorSize     = 0;
        std::uint32_t                       m_DSVDescriptorSize     = 0;
        std::uint32_t                       m_CBVDescriptorSize     = 0;

        DXGI_FORMAT                         m_BackBufferFormat      = DXGI_FORMAT_R8G8B8A8_UNORM;
        std::uint32_t                       m_SwapChainBufferCount  = 2;

        bool                                m_Use4xMSAA             = false;
        std::uint32_t                       m_4xMSAAQuality         = 0;
    };

} // namespace Spieler