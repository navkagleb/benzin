#include "bootstrap.hpp"

#include <spieler/core/entry_point.hpp>
#include <spieler/core/timer.hpp>
#include <spieler/core/layer_stack.hpp>
#include <spieler/core/imgui_layer.hpp>

#include <spieler/system/key_event.hpp>
#include <spieler/system/event_dispatcher.hpp>

#include <spieler/graphics/device.hpp>
#include <spieler/graphics/swap_chain.hpp>
#include <spieler/graphics/command_queue.hpp>

#include "layers/test_layer.hpp"
#include "layers/tessellation_layer.hpp"
#include "layers/instancing_and_culling_layer.hpp"

namespace sandbox
{

    class Application
    {
    public:
        Application()
        {
            m_MainWindow = std::make_unique<spieler::Window>("Sandbox", 1280, 720);
            m_MainWindow->SetEventCallbackFunction([&](spieler::Event& event) { WindowEventCallback(event); });

            m_Device = std::make_unique<spieler::Device>();
            m_CommandQueue = std::make_unique<spieler::CommandQueue>(*m_Device);
            m_SwapChain = std::make_unique<spieler::SwapChain>(*m_MainWindow, *m_Device, *m_CommandQueue);

            m_ImGuiLayer = m_LayerStack.PushOverlay<spieler::ImGuiLayer>(*m_MainWindow, *m_Device, *m_CommandQueue, *m_SwapChain);
            m_InstancingAndCullingLayer = m_LayerStack.Push<InstancingAndCullingLayer>(*m_MainWindow, *m_Device, *m_CommandQueue, *m_SwapChain);
        }

        void Execute()
        {
            m_FrameTimer.Reset();

            m_IsRunning = true;

            while (m_IsRunning)
            {
                m_MainWindow->ProcessEvents();

                if (m_IsPaused)
                {
                    ::Sleep(100);
                }
                else
                {
                    m_FrameTimer.Tick();

                    const float dt = m_FrameTimer.GetDeltaTime();

                    for (auto& layer : m_LayerStack)
                    {
                        layer->OnUpdate(dt);
                        layer->OnRender(dt);
                    }

                    m_ImGuiLayer->Begin();
                    {
                        for (auto& layer : m_LayerStack)
                        {
                            layer->OnImGuiRender(dt);
                        }
                    }
                    m_ImGuiLayer->End();

                    m_CommandQueue->Flush();
                    m_SwapChain->Flip(spieler::VSyncState::Disabled);
                }
            }
        }

    private:
        void WindowEventCallback(spieler::Event& event)
        {
            spieler::EventDispatcher dispatcher{ event };

            dispatcher.Dispatch<spieler::WindowCloseEvent>([this](spieler::WindowCloseEvent& event)
            {
                m_IsRunning = false;

                return false;
            });

            dispatcher.Dispatch<spieler::WindowEnterResizingEvent>([this](spieler::WindowEnterResizingEvent& event)
            {
                m_IsPaused = true;
                m_FrameTimer.Stop();

                return false;
            });

            dispatcher.Dispatch<spieler::WindowExitResizingEvent>([this](spieler::WindowExitResizingEvent& event)
            {
                m_IsPaused = false;
                m_FrameTimer.Start();

                return false;
            });

            dispatcher.Dispatch<spieler::WindowResizedEvent>([this](spieler::WindowResizedEvent& event)
            {
                m_CommandQueue->Flush();
                m_SwapChain->ResizeBackBuffers(*m_Device, event.GetWidth(), event.GetHeight());

                return false;
            });

            dispatcher.Dispatch<spieler::WindowFocusedEvent>([this](spieler::WindowFocusedEvent& event)
            {
                m_IsPaused = false;
                m_FrameTimer.Start();

                return false;
            });

            dispatcher.Dispatch<spieler::WindowUnfocusedEvent>([this](spieler::WindowUnfocusedEvent& event)
            {
                m_IsPaused = true;
                m_FrameTimer.Stop();

                return false;
            });

            for (auto it = m_LayerStack.ReverseBegin(); it != m_LayerStack.ReverseEnd(); ++it)
            {
                if (event.IsHandled())
                {
                    break;
                }

                (*it)->OnEvent(event);
            }
        }

    private:
        std::unique_ptr<spieler::Window> m_MainWindow;
        std::unique_ptr<spieler::Device> m_Device;
        std::unique_ptr<spieler::CommandQueue> m_CommandQueue;
        std::unique_ptr<spieler::SwapChain> m_SwapChain;

        spieler::Timer m_FrameTimer;
        spieler::LayerStack m_LayerStack;

        std::shared_ptr<spieler::ImGuiLayer> m_ImGuiLayer;
        std::shared_ptr<InstancingAndCullingLayer> m_InstancingAndCullingLayer;

        bool m_IsRunning{ false };
        bool m_IsPaused{ false };
    };

} // namespace sandbox

int spieler::ClientMain()
{
    {
        sandbox::Application application;
        application.Execute();
    }

    return 0;
}
