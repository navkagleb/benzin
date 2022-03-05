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

#include "renderer/renderer.h"
#include "renderer/resources/vertex_buffer.h"
#include "renderer/resources/index_buffer.h"
#include "renderer/constant_buffer.h"
#include "renderer/root_signature.h"
#include "renderer/shaders/shader.h"
#include "renderer/input_layout.h"
#include "renderer/geometry_generator.h"

namespace Spieler
{

    class Application
    {
        SINGLETON_IMPL(Application)

    public:
        Renderer& GetRenderer() { return m_Renderer; }
        const Renderer& GetRenderer() const { return m_Renderer; }

    public:
        bool Init(const std::string& title, std::uint32_t width, std::uint32_t height);
        void Shutdown();

        int Run();

    private:
        bool InitFence();
        bool InitMSAAQualitySupport();
        bool InitBackBufferRTV();
        bool InitDepthStencilTexture();
        bool InitViewport();
        bool InitScissor();
        bool InitImGui();
        bool InitPipelineState();

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
        struct PerObject
        {
            DirectX::XMMATRIX WorldViewProjectionMatrix{};
        };

    private:
        Window                              m_Window;
        Timer                               m_Timer;
        Renderer                            m_Renderer;

        VertexBuffer                        m_VertexBuffer;
        IndexBuffer                         m_IndexBuffer;
        VertexShader                        m_VertexShader;
        PixelShader                         m_PixelShader;

        ConstantBuffer<PerObject>           m_ConstantBuffer;
        RootSignature                       m_RootSignature;
        ComPtr<ID3D12PipelineState>         m_PipelineState;

        std::uint32_t                       m_FenceValue            = 0;
        ComPtr<ID3D12Fence>                 m_Fence;

        std::vector<ComPtr<ID3D12Resource>> m_SwapChainBuffers;
        ComPtr<ID3D12Resource>              m_DepthStencilBuffer;

        D3D12_VIEWPORT                      m_ScreenViewport{};
        RECT                                m_ScissorRect{};

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

        std::uint32_t                       m_IndexCount           = 0;
    };

} // namespace Spieler