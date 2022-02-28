#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

#include "common.h"
#include "window.h"
#include "timer.h"

#include "window_event.h"
#include "key_event.h"

namespace Spieler
{

    class Application
    {
        SINGLETON_IMPL(Application)

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
        bool InitSRVDescriptorHeap();
        bool InitDescriptorHeaps();
        bool InitBackBufferRTV();
        bool InitDepthStencilTexture();
        bool InitViewport();
        bool InitScissor();
        bool InitImGui();

        D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTV(std::uint32_t currentBackBuffer);
        D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView();

        void FlushCommandQueue();

        void OnUpdate(float dt);
        bool OnRender(float dt);
        void OnImGuiRender(float dt);

        void WindowEventCallback(Event& event);

        bool OnWindowClose(WindowCloseEvent& event);
        bool OnWindowMinimized(WindowMinimizedEvent& event);
        bool OnWindowMaximized(WindowMaximizedEvent& event);
        bool OnWindowRestored(WindowRestoredEvent& event);
        bool OnWindowEnterResizing(WindowEnterResizingEvent& event);
        bool OnWindowExitResizing(WindowExitResizingEvent& event);
        bool OnWindowFocused(WindowFocusedEvent& event);
        bool OnWindowUnfocused(WindowUnfocusedEvent& event);

        bool OnKeyPressed(KeyPressedEvent& event);

        void Close();
        void Resize();

        void CalcStats();

    private:
        Window                              m_Window;
        Timer                               m_Timer;

        ComPtr<ID3D12Device>                m_Device;

        std::uint32_t                       m_FenceValue            = 0;
        ComPtr<ID3D12Fence>                 m_Fence;

        ComPtr<ID3D12CommandQueue>          m_CommandQueue;
        ComPtr<ID3D12CommandAllocator>      m_CommandAllocator;
        ComPtr<ID3D12GraphicsCommandList>   m_CommandList;
        ComPtr<IDXGIFactory>                m_DXGIFactory;
        ComPtr<IDXGISwapChain>              m_SwapChain;
        ComPtr<IDXGISwapChain3>             m_SwapChain3;
        ComPtr<ID3D12DescriptorHeap>        m_RTVDescriptorHeap;
        ComPtr<ID3D12DescriptorHeap>        m_DSVDescriptorHeap;
        ComPtr<ID3D12DescriptorHeap>        m_SRVDescriptorHeap;
        std::vector<ComPtr<ID3D12Resource>> m_SwapChainBuffers;
        ComPtr<ID3D12Resource>              m_DepthStencilBuffer;

        D3D12_VIEWPORT                      m_ScreenViewport{};
        RECT                                m_ScissorRect{};

        std::uint32_t                       m_RTVDescriptorSize     = 0;
        std::uint32_t                       m_DSVDescriptorSize     = 0;
        std::uint32_t                       m_CBVDescriptorSize     = 0;
        std::uint32_t                       m_SRVDescriptorSize     = 0;

        DXGI_FORMAT                         m_BackBufferFormat      = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT                         m_DepthStencilFormat    = DXGI_FORMAT_D24_UNORM_S8_UINT;
        std::uint32_t                       m_SwapChainBufferCount  = 2;

        bool                                m_Use4xMSAA             = false;
        std::uint32_t                       m_4xMSAAQuality         = 0;

        std::string                         m_WindowTitle;

        bool                                m_IsRunning             = false;
        bool                                m_IsPaused              = false;
        bool                                m_IsResizing            = false;
        bool                                m_IsMinimized           = false;
        bool                                m_IsMaximized           = false;
        bool                                m_IsFullscreen          = false;

        DirectX::XMFLOAT4                   m_ClearColor            = { 0.0f, 0.0f, 0.0f, 1.0f };

        bool                                m_IsShowDemoWindow      = false;
    };

} // namespace Spieler