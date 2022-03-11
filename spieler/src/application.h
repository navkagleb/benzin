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
#include "renderer/descriptor_heap.h"

#include "renderer/resources/vertex_buffer.h"
#include "renderer/resources/index_buffer.h"
#include "renderer/resources/constant_buffer.h"

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

        int Run();

    private:
        bool InitMSAAQualitySupport();
        bool InitImGui();
        bool InitPipelineState();

        void InitViewMatrix();
        void InitProjectionMatrix();
        void InitViewport();
        void InitScissorRect();

        void OnUpdate(float dt);
        bool OnRender(float dt);
        void OnImGuiRender(float dt);
        void OnResize();
        void OnClose();

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

        void CalcStats();

    private:
        struct PerObject
        {
            DirectX::XMMATRIX WorldViewProjectionMatrix{};
        };

    private:
        Timer                               m_Timer;
        Window                              m_Window;
        Renderer                            m_Renderer;

        DescriptorHeap                      m_CBVDescriptorHeap;

        VertexBuffer                        m_VertexBuffer;
        IndexBuffer                         m_IndexBuffer;
        VertexShader                        m_VertexShader;
        PixelShader                         m_PixelShader;
        ConstantBuffer<PerObject>           m_ConstantBuffer;
        RootSignature                       m_RootSignature;
        ComPtr<ID3D12PipelineState>         m_PipelineState;

        Viewport                            m_Viewport;
        ScissorRect                         m_ScissorRect;

        bool                                m_Use4xMSAA             = false;
        std::uint32_t                       m_4xMSAAQuality         = 0;

        std::string                         m_WindowTitle;

        bool                                m_IsRunning             = false;
        bool                                m_IsPaused              = false;
        bool                                m_IsResizing            = false;
        bool                                m_IsMinimized           = false;
        bool                                m_IsMaximized           = false;
        bool                                m_IsFullscreen          = false;

        DirectX::XMFLOAT4                   m_ClearColor            = { 0.1f, 0.0f, 0.3f, 1.0f };

        bool                                m_IsShowDemoWindow      = false;

        std::uint32_t                       m_IndexCount           = 0;

        // View
        DirectX::XMVECTOR                   m_CameraPosition{};
        DirectX::XMVECTOR                   m_CameraTarget{};
        DirectX::XMVECTOR                   m_CameraUpDirection{};
        DirectX::XMMATRIX                   m_View;

        // Projection
        DirectX::XMMATRIX                   m_Projection{};
    };

} // namespace Spieler