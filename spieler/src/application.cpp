#include "application.hpp"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>

#include "window/event_dispatcher.hpp"
#include "window/window_event.hpp"
#include "window/key_event.hpp"

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

    Application::~Application()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    bool Application::InitInternal(const std::string& title, std::uint32_t width, std::uint32_t height)
    {
#if defined(SPIELER_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

        SPIELER_RETURN_IF_FAILED(InitWindow(title, width, height));
        SPIELER_RETURN_IF_FAILED(InitRenderer());
        SPIELER_RETURN_IF_FAILED(InitImGui());
        SPIELER_RETURN_IF_FAILED(InitExternal());

        UpdateScreenViewport();
        UpdateScreenScissorRect();

        return true;
    }

    int Application::Run()
    {
        MSG message{};

        m_Timer.Reset();

        m_ApplicationProps.IsRunning = true;

        while (m_ApplicationProps.IsRunning && message.message != WM_QUIT)
        {
            while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&message);
                ::DispatchMessage(&message);
            }
            
            if (m_ApplicationProps.IsPaused)
            {
                ::Sleep(100);
            }
            else
            {
                m_Timer.Tick();

                const float dt = m_Timer.GetDeltaTime();

                ImGui_ImplDX12_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                OnUpdate(dt);
                OnImGuiRender(dt);
                ImGui::Render();

                if (!OnRender(dt))
                {
                    break;
                }
            }
        }

        m_Renderer.~Renderer();

        return static_cast<int>(message.lParam);
    }

    bool Application::InitWindow(const std::string& title, std::uint32_t width, std::uint32_t height)
    {
        SPIELER_RETURN_IF_FAILED(m_Window.Init(title, width, height));

        m_Window.SetEventCallbackFunction([&](Event& event) { WindowEventCallback(event); });

        return true;
    }

    bool Application::InitRenderer()
    {
        SPIELER_RETURN_IF_FAILED(m_Renderer.Init(m_Window));

        return true;
    }

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

        SPIELER_RETURN_IF_FAILED(m_ImGuiDescriptorHeap.Init(DescriptorHeapType::CBVSRVUAV, 1));

        // Setup Platform/Renderer backends
        SPIELER_RETURN_IF_FAILED(ImGui_ImplWin32_Init(m_Window.GetHandle()));
        SPIELER_RETURN_IF_FAILED(ImGui_ImplDX12_Init(
            m_Renderer.m_Device.Get(),
            1,
            m_Renderer.GetSwapChainProps().BufferFormat, 
            static_cast<ID3D12DescriptorHeap*>(m_ImGuiDescriptorHeap),
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_ImGuiDescriptorHeap.GetDescriptorCPUHandle(0) },
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_ImGuiDescriptorHeap.GetDescriptorGPUHandle(0) }
        ));

        return true;
    }

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

            const D3D12_CPU_DESCRIPTOR_HANDLE a{ m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetDescriptorCPUHandle(currentBackBuffer) };
            const D3D12_CPU_DESCRIPTOR_HANDLE b{ m_Renderer.m_DSVDescriptorHeap.GetDescriptorCPUHandle(0) };

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

    void Application::OnImGuiRender(float dt)
    {
        m_LayerStack.OnImGuiRender(dt);
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
        m_ApplicationProps.IsRunning = false;

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