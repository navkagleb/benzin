#include "bootstrap.hpp"

#include <benzin/core/entry_point.hpp>
#include <benzin/core/imgui_layer.hpp>
#include <benzin/core/layer_stack.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/system/key_event.hpp>

#include "main_layer.hpp"
#include "raytracing_layer.hpp"

namespace sandbox
{

    class Application
    {
    public:
        Application()
        {
            m_MainWindow = std::make_unique<benzin::Window>("Benzin: Sandbox", 1280, 720, [&](benzin::Event& event) { WindowEventCallback(event); });
            m_Backend = std::make_unique<benzin::Backend>();
            m_Device = std::make_unique<benzin::Device>(*m_Backend);
            m_SwapChain = std::make_unique<benzin::SwapChain>(*m_MainWindow, *m_Backend, *m_Device);

            const benzin::GraphicsRefs graphicsRefs
            {
                .WindowRef = *m_MainWindow,
                .BackendRef = *m_Backend,
                .DeviceRef = *m_Device,
                .SwapChainRef = *m_SwapChain,
            };

            BeginFrame();
            {
                m_ImGuiLayer = m_LayerStack.PushOverlay<benzin::ImGuiLayer>(graphicsRefs);
                m_MainLayer = m_LayerStack.Push<MainLayer>(graphicsRefs);
                //m_RaytracingLayer = m_LayerStack.Push<RaytracingLayer>(graphicsRefs);
            }
            EndFrame();

            BenzinAssert(m_ImGuiLayer.get());
        }
        
        void Execute()
        {
            using namespace std::literals::chrono_literals;

            benzin::Layer::OnStaticBeforeMainLoop();

            m_IsRunning = true;

            while (m_IsRunning)
            {
                m_MainWindow->ProcessEvents();

                if (m_IsPaused)
                {
                    std::this_thread::sleep_for(100ms);
                }
                else
                {
                    BeginFrame();
                    ProcessFrame();
                    EndFrame();
                }
            }
        }

    private:
        void WindowEventCallback(benzin::Event& event)
        {
            benzin::EventDispatcher dispatcher{ event };
            dispatcher.Dispatch(&Application::OnWindowClose, *this);
            dispatcher.Dispatch(&Application::OnWindowEnterResizing, *this);
            dispatcher.Dispatch(&Application::OnWindowExitResizing, *this);
            dispatcher.Dispatch(&Application::OnWindowResized, *this);
            dispatcher.Dispatch(&Application::OnKeyPressed, *this);

            for (auto& layer : m_LayerStack | std::views::reverse)
            {
                if (event.IsHandled())
                {
                    break;
                }

                layer->OnEvent(event);
            }
        }

        void BeginFrame()
        {
            m_Device->GetGraphicsCommandQueue().ResetCommandList(m_SwapChain->GetCurrentFrameIndex());
        }

        void ProcessFrame()
        {
            benzin::Layer::OnStaticUpdate();

            for (auto& layer : m_LayerStack)
            {
                layer->OnBeginFrame();
                layer->OnUpdate();
                layer->OnRender();
                layer->OnEndFrame();
            }

            if (m_ImGuiLayer->IsWidgetDrawEnabled())
            {
                m_ImGuiLayer->Begin();
                {
                    for (auto& layer : m_LayerStack)
                    {
                        layer->OnImGuiRender();
                    }
                }
                m_ImGuiLayer->End();
            }
        }

        void EndFrame()
        {
            benzin::Layer::OnStaticEndFrame();

            m_Device->GetGraphicsCommandQueue().ExecuteCommandList();
            m_SwapChain->Flip();
        }

    private:
        bool OnWindowClose(benzin::WindowCloseEvent& event)
        {
            m_IsRunning = false;
            return false;
        };

        bool OnWindowEnterResizing(benzin::WindowEnterResizingEvent& event)
        {
            m_IsPaused = true;
            benzin::Layer::s_FrameTimer.Stop();

            return false;
        };

        bool OnWindowExitResizing(benzin::WindowExitResizingEvent& event)
        {
            m_IsPaused = false;
            benzin::Layer::s_FrameTimer.Start();

            return false;
        };

        bool OnWindowResized(benzin::WindowResizedEvent& event)
        {
            m_SwapChain->ResizeBackBuffers(event.GetWidth(), event.GetHeight());
            return false;
        };

        bool OnKeyPressed(benzin::KeyPressedEvent& event)
        {
            if (event.GetKeyCode() == benzin::KeyCode::Escape)
            {
                m_IsRunning = false;
            }

            return false;
        }

    private:
        std::unique_ptr<benzin::Window> m_MainWindow;
        std::unique_ptr<benzin::Backend> m_Backend;
        std::unique_ptr<benzin::Device> m_Device;
        std::unique_ptr<benzin::SwapChain> m_SwapChain;

        benzin::LayerStack m_LayerStack;

        std::shared_ptr<benzin::ImGuiLayer> m_ImGuiLayer;
        std::shared_ptr<MainLayer> m_MainLayer;
        std::shared_ptr<RaytracingLayer> m_RaytracingLayer;

        bool m_IsRunning = false;
        bool m_IsPaused = false;
    };

} // namespace sandbox

int benzin::ClientMain()
{
#if 1
    sandbox::Application application;
    application.Execute();
#else
    uint8_t bits = ~3;

    std::println("{}", std::bitset<8>(bits).to_string());

    for (auto i : std::views::iota(0, 10))
    {
        std::println("i: {}, aligned i: {}", i, i & ~3);
    }

    BenzinExecuteOnScopeExit([] { std::println("out of scope"); });
    std::println("before scope ends");
#endif

    return 0;
}
