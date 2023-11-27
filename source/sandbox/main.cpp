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
#include "raytracing_hello_triangle_layer.hpp"

namespace sandbox
{

    class Application
    {
    public:
        Application()
        {
            const benzin::WindowCreation windowCreation
            {
                .Title = "Benzin: Sandbox",
                .Width = 1280,
                .Height = 720,
                .EventCallback = [&](benzin::Event& event) { WindowEventCallback(event); },
            };

            m_MainWindow = std::make_unique<benzin::Window>(windowCreation);
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

#if 0
                m_MainLayer = m_LayerStack.Push<MainLayer>(graphicsRefs);
#else
                m_RaytracingHelloTriangleLayer = m_LayerStack.Push<RaytracingHelloTriangleLayer>(graphicsRefs);
#endif
            }
            EndFrame();
        }

        ~Application()
        {
            // TODO: Need to flush until deferred release is implemented
            m_Device->GetGraphicsCommandQueue().Flush();
        }
        
        void ExecuteMainLoop()
        {
            benzin::Layer::OnStaticBeforeMainLoop();

            m_IsRunning = true;
            while (m_IsRunning)
            {
                m_MainWindow->ProcessEvents();

                BeginFrame();
                ProcessFrame();
                EndFrame();
            }

            benzin::Layer::OnStaticAfterMainLoop();
        }

    private:
        void WindowEventCallback(benzin::Event& event)
        {
            benzin::EventDispatcher dispatcher{ event };
            dispatcher.Dispatch(&Application::OnWindowClose, *this);
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

            benzin::Layer::OnStaticBeginFrame();
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
            m_SwapChain->SwapBackBuffer();
        }

    private:
        bool OnWindowClose(benzin::WindowCloseEvent& event)
        {
            m_IsRunning = false;
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
        std::shared_ptr<RaytracingHelloTriangleLayer> m_RaytracingHelloTriangleLayer;

        bool m_IsRunning = false;
    };

} // namespace sandbox

int benzin::ClientMain()
{
#if 1
    benzin::GraphicsSettings::Initialize(
    {
        .MainAdapterIndex = 1,
        .FrameInFlightCount = 3,
        .DebugLayerParams
        {
            .IsGPUBasedValidationEnabled = true,
        },
    });

    sandbox::Application application;
    application.ExecuteMainLoop();
#else
    const auto s = BenzinAsS(1.0f);
    const auto ms = BenzinAsMS(s);

    const benzin::MilliSeconds ms2{ 100.0f };
    const benzin::Seconds s2 = ms2;
#endif

    return 0;
}
