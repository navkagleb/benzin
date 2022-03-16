#include "application.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>

#include "window/event_dispatcher.h"
#include "window/window_event.h"
#include "window/key_event.h"

namespace Spieler
{

    Application& Application::GetInstance()
    {
        SPIELER_ASSERT(g_Instance);

        return *g_Instance;
    }

    Application::Application()
    {
        if (!g_Instance)
        {
            g_Instance = this;
        }
    }

    bool Application::InitInternal(const std::string& title, std::uint32_t width, std::uint32_t height)
    {
#if defined(SPIELER_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

        SPIELER_RETURN_IF_FAILED(InitWindow(title, width, height));
        SPIELER_RETURN_IF_FAILED(m_Renderer.Init(m_Window));
        SPIELER_RETURN_IF_FAILED(m_ImGuiDescriptorHeap.Init(DescriptorHeapType_CBVSRVUAV, 1));

#if SPIELER_USE_IMGUI
        SPIELER_RETURN_IF_FAILED(InitImGui());
#endif

        UpdateScreenViewport();
        UpdateScreenScissorRect();

        SPIELER_RETURN_IF_FAILED(InitExternal());

        return true;
    }

    int Application::Run()
    {
        MSG message{};

        m_Timer.Reset();

        m_ApplicationProps.IsRunning = true;

        while (m_ApplicationProps.IsRunning && message.message != WM_QUIT)
        {
            if (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&message);
                ::DispatchMessage(&message);
            }
            else
            {
                if (m_ApplicationProps.IsPaused)
                {
                    ::Sleep(100);
                }
                else
                {
                    m_Timer.Tick();

                    const float dt = m_Timer.GetDeltaTime();

#if SPIELER_USE_IMGUI
                    // Start the Dear ImGui frame
                    ImGui_ImplDX12_NewFrame();
                    ImGui_ImplWin32_NewFrame();
                    ImGui::NewFrame();
#endif

                    OnUpdate(dt);

#if SPIELER_USE_IMGUI
                    OnImGuiRender(dt);
                    ImGui::Render();
#endif

                    if (!OnRender(dt))
                    {
                        break;
                    }
                }
            }
        }

        return static_cast<int>(message.lParam);
    }

    bool Application::InitWindow(const std::string& title, std::uint32_t width, std::uint32_t height)
    {
        SPIELER_RETURN_IF_FAILED(m_Window.Init(title, width, height));

        m_Window.SetEventCallbackFunction([&](Event& event) { WindowEventCallback(event); });

        return true;
    }

#if SPIELER_USE_IMGUI
    bool Application::InitImGui()
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(m_Window.GetHandle());
        ImGui_ImplDX12_Init(
            m_Renderer.m_Device.Get(),
            1,
            m_Renderer.GetSwapChainProps().BufferFormat, 
            static_cast<ID3D12DescriptorHeap*>(m_ImGuiDescriptorHeap),
            m_ImGuiDescriptorHeap.GetCPUHandle(0),
            m_ImGuiDescriptorHeap.GetGPUHandle(0)
        );

        return true;
    }
#endif

    void Application::OnUpdate(float dt)
    {
        CalcStats();

        m_LayerStack.OnUpdate(dt);
    }

    bool Application::OnRender(float dt)
    {
        SPIELER_RETURN_IF_FAILED(m_LayerStack.OnRender(dt));

        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList());
        {
            m_Renderer.SetViewport(m_ScreenViewport);
            m_Renderer.SetScissorRect(m_ScreenScissorRect);

            const std::uint32_t currentBackBuffer = m_Renderer.m_SwapChain3->GetCurrentBackBufferIndex();

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource    = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_RENDER_TARGET;

            m_Renderer.m_CommandList->ResourceBarrier(1, &barrier);

            auto a = m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetCPUHandle(currentBackBuffer);
            auto b = m_Renderer.m_DSVDescriptorHeap.GetCPUHandle(0);

            m_Renderer.m_CommandList->OMSetRenderTargets(
                1,
                &a,
                true,
                &b
            );

            m_ImGuiDescriptorHeap.Bind();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_Renderer.m_CommandList.Get());

            barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource    = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_PRESENT;

            m_Renderer.m_CommandList->ResourceBarrier(1, &barrier);
        }
        SPIELER_RETURN_IF_FAILED(m_Renderer.ExexuteCommandList(false));

        SPIELER_RETURN_IF_FAILED(m_Renderer.SwapBuffers(VSyncState_Disable));
        SPIELER_RETURN_IF_FAILED(m_Renderer.FlushCommandQueue());

        return true;
    }

#if SPIELER_USE_IMGUI
    void Application::OnImGuiRender(float dt)
    {
        m_LayerStack.OnImGuiRender(dt);
    }
#endif

    void Application::OnClose()
    {
        m_ApplicationProps.IsRunning = false;

#if SPIELER_USE_IMGUI
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
#endif
    }

    void Application::WindowEventCallback(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowClose));
        dispatcher.Dispatch<WindowMinimizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowMinimized));
        dispatcher.Dispatch<WindowMaximizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowMaximized));
        dispatcher.Dispatch<WindowRestoredEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowRestored));
        dispatcher.Dispatch<WindowEnterResizingEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowEnterResizing));
        dispatcher.Dispatch<WindowExitResizingEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowExitResizing));
        dispatcher.Dispatch<WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));
        dispatcher.Dispatch<WindowFocusedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowFocused));
        dispatcher.Dispatch<WindowUnfocusedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowUnfocused));

        m_LayerStack.OnEvent(event);
    }

    bool Application::OnWindowClose(WindowCloseEvent& event)
    {
        OnClose();

        return false;
    }

    bool Application::OnWindowMinimized(WindowMinimizedEvent& event)
    {
        m_ApplicationProps.IsPaused = true;

        return false;
    }

    bool Application::OnWindowMaximized(WindowMaximizedEvent& event)
    {
        m_ApplicationProps.IsPaused = false;

        return false;
    }

    bool Application::OnWindowRestored(WindowRestoredEvent& event)
    {
        m_ApplicationProps.IsPaused = false;

        return false;
    }

    bool Application::OnWindowEnterResizing(WindowEnterResizingEvent& event)
    {
        m_ApplicationProps.IsPaused = true;

        m_Timer.Stop();

        return false;
    }

    bool Application::OnWindowExitResizing(WindowExitResizingEvent& event)
    {
        m_ApplicationProps.IsPaused = false;

        m_Timer.Start();

        return false;
    }

    bool Application::OnWindowResized(WindowResizedEvent& event)
    {
        m_Renderer.ResizeBuffers(m_Window.GetWidth(), m_Window.GetHeight());

        UpdateScreenViewport();
        UpdateScreenScissorRect();

        return false;
    }

    bool Application::OnWindowFocused(WindowFocusedEvent& event)
    {
        m_ApplicationProps.IsPaused = false;

        m_Timer.Start();

        return false;
    }

    bool Application::OnWindowUnfocused(WindowUnfocusedEvent& event)
    {
        m_ApplicationProps.IsPaused = true;

        m_Timer.Stop();

        return false;
    }

    void Application::CalcStats()
    {
        static const float limit        = 1.0f;
        static const float coefficient  = 1.0f / limit;

        static std::uint32_t    fps         = 0;
        static float            timeElapsed = 0.0f;

        fps++;

        if (float delta = m_Timer.GetTotalTime() - timeElapsed; delta >= limit)
        {
            m_Window.SetTitle("FPS: " + std::to_string(fps * coefficient) + " ms: " + std::to_string(1000.0f / coefficient / fps));

            fps          = 0;
            timeElapsed += delta;
        }
    }

    void Application::UpdateScreenViewport()
    {
        m_ScreenViewport.X          = 0.0f;
        m_ScreenViewport.Y          = 0.0f;
        m_ScreenViewport.Width      = static_cast<float>(m_Window.GetWidth());
        m_ScreenViewport.Height     = static_cast<float>(m_Window.GetHeight());
        m_ScreenViewport.MinDepth   = 0.0f;
        m_ScreenViewport.MaxDepth   = 1.0f;
    }

    void Application::UpdateScreenScissorRect()
    {
        m_ScreenScissorRect.X         = 0.0f;
        m_ScreenScissorRect.Y         = 0.0f;
        m_ScreenScissorRect.Width     = static_cast<float>(m_Window.GetWidth());
        m_ScreenScissorRect.Height    = static_cast<float>(m_Window.GetHeight());
    }

} // namespace Spieler