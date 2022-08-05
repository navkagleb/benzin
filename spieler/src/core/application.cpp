#include "spieler/config/bootstrap.hpp"

#include "spieler/core/application.hpp"

#include <third_party/fmt/format.h>

#include "spieler/core/common.hpp"
#include "spieler/core/logger.hpp"

#include "spieler/system/event_dispatcher.hpp"

#include "platform/win64/win64_window.hpp"
#include "platform/win64/win64_input.hpp"
#include "platform/dx12/dx12_common.hpp"

namespace spieler
{

    static Application* g_Instance{ nullptr };

    void Application::CreateInstance()
    {
        SPIELER_ASSERT(!g_Instance);

        CreateApplication();
    }

    Application& Application::GetInstance()
    {
        SPIELER_ASSERT(g_Instance);

        return *g_Instance;
    }

    void Application::DestroyInstance()
    {
        delete g_Instance;
        g_Instance = nullptr;
    }

    Application::Application(const Config& config)
    {
        SPIELER_ASSERT(!g_Instance);

        g_Instance = this;

#if defined(SPIELER_PLATFORM_WINDOWS)
        m_Window = std::make_unique<Win64_Window>(config.Title, config.Width, config.Height);
        m_Window->SetEventCallbackFunction([&](Event& event) { WindowEventCallback(event); });
#else
    static_assert(false);
#endif

        renderer::Renderer::CreateInstance(*m_Window);

        m_ImGuiLayer = m_LayerStack.PushOverlay<ImGuiLayer>();
    }

    Application::~Application()
    {
        renderer::Renderer::DestroyInstance();
    }

    void Application::Run()
    {
        m_Timer.Reset();

        m_IsRunning = true;

        renderer::Renderer& renderer{ renderer::Renderer::GetInstance() };
        renderer::Context& context{ renderer.GetContext() };

        while (m_IsRunning)
        {
            m_Window->ProcessEvents();
            
            if (m_IsPaused)
            {
                ::Sleep(100);
            }
            else
            {
                m_Timer.Tick();

                const float dt{ m_Timer.GetDeltaTime() };

                context.ResetCommandAllocator();

                for (auto& layer : m_LayerStack)
                {
                    layer->OnUpdate(dt);
                    layer->OnRender(dt);
                }

                ImGuiLayer::Begin();
                {
                    for (auto& layer : m_LayerStack)
                    {
                        layer->OnImGuiRender(dt);
                    }
                }
                ImGuiLayer::End();

                SPIELER_ASSERT(renderer.Present(renderer::VSyncState::Disabled));
            }
        }
    }

    void Application::WindowEventCallback(Event& event)
    {
        EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<WindowCloseEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowClose));
        dispatcher.Dispatch<WindowMinimizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowMinimized));
        dispatcher.Dispatch<WindowMaximizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowMaximized));
        dispatcher.Dispatch<WindowRestoredEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowRestored));
        dispatcher.Dispatch<WindowEnterResizingEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowEnterResizing));
        dispatcher.Dispatch<WindowExitResizingEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowExitResizing));
        dispatcher.Dispatch<WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));
        dispatcher.Dispatch<WindowFocusedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowFocused));
        dispatcher.Dispatch<WindowUnfocusedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowUnfocused));

        for (auto it = m_LayerStack.ReverseBegin(); it != m_LayerStack.ReverseEnd(); ++it)
        {
            if (event.IsHandled())
            {
                break;
            }

            (*it)->OnEvent(event);
        }
    }

    bool Application::OnWindowClose(WindowCloseEvent& event)
    {
        m_IsRunning = false;

        return false;
    }

    bool Application::OnWindowMinimized(WindowMinimizedEvent& event)
    {
        m_IsPaused = true;

        return false;
    }

    bool Application::OnWindowMaximized(WindowMaximizedEvent& event)
    {
        m_IsPaused = false;

        return false;
    }

    bool Application::OnWindowRestored(WindowRestoredEvent& event)
    {
        m_IsPaused = false;

        return false;
    }

    bool Application::OnWindowEnterResizing(WindowEnterResizingEvent& event)
    {
        m_IsPaused = true;

        m_Timer.Stop();

        return false;
    }

    bool Application::OnWindowExitResizing(WindowExitResizingEvent& event)
    {
        m_IsPaused = false;

        m_Timer.Start();

        return false;
    }

    bool Application::OnWindowResized(WindowResizedEvent& event)
    {
        renderer::Renderer::GetInstance().ResizeBuffers(m_Window->GetWidth(), m_Window->GetHeight());

        return false;
    }

    bool Application::OnWindowFocused(WindowFocusedEvent& event)
    {
        m_IsPaused = false;

        m_Timer.Start();

        return false;
    }

    bool Application::OnWindowUnfocused(WindowUnfocusedEvent& event)
    {
        m_IsPaused = true;

        m_Timer.Stop();

        return false;
    }

} // namespace spieler
