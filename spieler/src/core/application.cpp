#include "spieler/config/bootstrap.hpp"

#include "spieler/core/application.hpp"

#include <third_party/fmt/format.h>

#include "platform/win64/win64_window.hpp"
#include "platform/win64/win64_input.hpp"
#include "platform/dx12/dx12_common.hpp"

#include "spieler/core/common.hpp"
#include "spieler/core/logger.hpp"

#include "spieler/system/event_dispatcher.hpp"

namespace spieler
{

    Application* Application::GetInstance()
    {
        SPIELER_ASSERT(g_Instance);

        return g_Instance;
    }

    Application::Application()
    {
        SPIELER_ASSERT(!g_Instance);
    }

    Application::~Application()
    {
        //m_ImGuiLayer.reset();
        //m_LayerStack.~LayerStack();
        renderer::Renderer::DestoryInstance();
    }

    bool Application::InitInternal(const std::string& title, uint32_t width, uint32_t height)
    {
#if 0
#if defined(SPIELER_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
#endif

        // TODO: Init "Subsystems"
        SPIELER_RETURN_IF_FAILED(InitWindow(title, width, height));
        renderer::Renderer::CreateInstance(*g_Instance->m_Window);

        m_LayerStack.PushOverlay<ImGuiLayer>();

        SPIELER_RETURN_IF_FAILED(InitExternal());

        return true;
    }

    void Application::Run()
    {
        m_Timer.Reset();

        m_ApplicationProps.IsRunning = true;

        renderer::Renderer& renderer{ renderer::Renderer::GetInstance() };
        renderer::Context& context{ renderer.GetContext() };

        while (m_ApplicationProps.IsRunning)
        {
            m_Window->ProcessEvents();
            
            if (m_ApplicationProps.IsPaused)
            {
                ::Sleep(100);
            }
            else
            {
                m_Timer.Tick();

                const float dt{ m_Timer.GetDeltaTime() };

                CalcStats(dt);

                SPIELER_ASSERT(context.ResetCommandAllocator());

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

    bool Application::InitWindow(const std::string& title, uint32_t width, uint32_t height)
    {
#if defined(SPIELER_PLATFORM_WINDOWS)
        m_Window = std::make_unique<Win64_Window>(title, width, height);
#else
        static_assert(false);
#endif

        m_Window->SetEventCallbackFunction([&](Event& event) { WindowEventCallback(event); });

        return true;
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
        renderer::Renderer& renderer{ renderer::Renderer::GetInstance() };
        renderer.ResizeBuffers(m_Window->GetWidth(), m_Window->GetHeight());

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

    void Application::CalcStats(float dt)
    {
        const float fps{ 1.0f / dt };

        m_Window->SetTitle(fmt::format("FPS: {:.2f}, ms: {:.2f}", fps, dt * 1000.0f));
    }

} // namespace spieler