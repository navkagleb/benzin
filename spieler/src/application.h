#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include <d3d12.h>
#include <dxgi.h>

#include "common.h"
#include "window.h"
#include "timer.h"

namespace Spieler
{

    class Application
    {
        SINGLETON_IMPL(Application)

    private:
        friend static LRESULT CALLBACK EventCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
        bool InitBackBufferRTV();
        bool InitDepthStencilTexture();
        bool InitViewport();
        bool InitScissor();

        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTV();
        D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView();

        void FlushCommandQueue();

        void OnUpdate(float dt);
        void OnRender(float dt);
        void OnResize();

        void CalcStats();

    private:
        WindowsRegisterClass                m_WindowsRegisterClass;
        Window                              m_Window;
        Window                              m_Window1;
        Timer                               m_Timer;

        ComPtr<ID3D12Device>                m_Device;

        std::uint32_t                       m_FenceValue            = 0;
        ComPtr<ID3D12Fence>                 m_Fence;

        ComPtr<ID3D12CommandQueue>          m_CommandQueue;
        ComPtr<ID3D12CommandAllocator>      m_CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList>   m_CommandList;
        ComPtr<IDXGIFactory>                m_DXGIFactory;
        ComPtr<IDXGISwapChain>              m_SwapChain;
        ComPtr<ID3D12DescriptorHeap>        m_RTVDescriptorHeap;
        ComPtr<ID3D12DescriptorHeap>        m_DSVDescriptorHeap;
        std::vector<ComPtr<ID3D12Resource>> m_SwapChainBuffers;
        ComPtr<ID3D12Resource>              m_DepthStencilBuffer;

        D3D12_VIEWPORT                      m_ScreenViewport{};
        RECT                                m_ScissorRect{};

        std::uint32_t                       m_CurrentBackBuffer     = 0;

        std::uint32_t                       m_RTVDescriptorSize     = 0;
        std::uint32_t                       m_DSVDescriptorSize     = 0;
        std::uint32_t                       m_CBVDescriptorSize     = 0;

        DXGI_FORMAT                         m_BackBufferFormat      = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT                         m_DepthStencilFormat    = DXGI_FORMAT_D24_UNORM_S8_UINT;
        std::uint32_t                       m_SwapChainBufferCount  = 2;

        bool                                m_Use4xMSAA             = false;
        std::uint32_t                       m_4xMSAAQuality         = 0;

        std::string                         m_WindowTitle;

        bool                                m_IsPaused              = false;
        bool                                m_IsResizing            = false;
    };

} // namespace Spieler