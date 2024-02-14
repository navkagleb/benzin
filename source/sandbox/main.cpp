#include "bootstrap.hpp"

#include <benzin/core/command_line_args.hpp>
#include <benzin/core/entry_point.hpp>
#include <benzin/core/imgui_layer.hpp>
#include <benzin/core/layer_stack.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/system/key_event.hpp>

#include "scene_layer.hpp"
#include "rt_hello_triangle_layer.hpp"
#include "rt_procedural_geometry_layer.hpp"

namespace sandbox
{

    class FrameRateCounter
    {
    public:
        auto GetFrameRate() const { return m_FrameRate; }
        auto GetDeltaTime() const { return benzin::SecToUS(1.0f / m_FrameRate); }

        bool IsIntervalPassed() const { return m_ElapsedTime >= benzin::Layer::s_UpdateStatsInterval; }

        void UpdateStats()
        {
            BenzinAssert(IsIntervalPassed());

            m_FrameRate = m_ElapsedFrameCount / benzin::ToFloatSec(m_ElapsedTime);

            m_ElapsedTime -= benzin::Layer::s_UpdateStatsInterval;
            m_ElapsedFrameCount = 0;
        }

        void OnUpdate(std::chrono::microseconds dt)
        {
            m_ElapsedTime += benzin::ToMS(dt);
            m_ElapsedFrameCount++;
        }

    private:
        float m_FrameRate = 60.0f;

        std::chrono::milliseconds m_ElapsedTime = std::chrono::milliseconds::zero();
        uint32_t m_ElapsedFrameCount = 0;
    };

    class Application
    {
    public:
        Application()
            : m_FrameTimerRef{ benzin::Layer::s_FrameTimer }
            , m_IsUpdateStatsIntervalPassedRef{ benzin::Layer::s_IsUpdateStatsIntervalPassed }
        {
            BenzinLogTimeOnScopeExit("Create Application");

            const auto commandLineArgs = benzin::CommandLineArgsInstance::Get();

            const benzin::WindowCreation windowCreation
            {
                .Title = "Benzin: Sandbox",
                .Width = commandLineArgs.GetWindowWidth(),
                .Height = commandLineArgs.GetWindowHeight(),
                .IsResizable = commandLineArgs.IsWindowResizable(),
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

#if 1
                m_SceneLayer = m_LayerStack.Push<SceneLayer>(graphicsRefs);
#elif 0
                m_RTHelloTriangleLayer = m_LayerStack.Push<RTHelloTriangleLayer>(graphicsRefs);
#else
                m_RTProceduralGeometryLayer = m_LayerStack.Push<RTProceduralGeometryLayer>(graphicsRefs);
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
            m_IsRunning = true;
            m_FrameTimerRef.Reset();

            while (m_IsRunning)
            {
                m_MainWindow->ProcessEvents();

                BeginFrame();
                ProcessFrame();
                EndFrame();

                if (m_IsUpdateStatsIntervalPassedRef = m_FrameRateCounter.IsIntervalPassed())
                {
                    m_FrameRateCounter.UpdateStats();

                    m_ImGuiLayer->SetFrameRateStats(m_FrameRateCounter.GetFrameRate(), benzin::ToFloatMS(m_FrameRateCounter.GetDeltaTime()));
                    m_ImGuiLayer->SetApplicationTimings(m_Timings);
                }
            }
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
            BenzinGrabTimeOnScopeExit(m_Timings[benzin::ApplicationTiming::BeginFrame]);

            m_Device->GetGraphicsCommandQueue().ResetCommandList(m_Device->GetActiveFrameIndex());
        }

        void ProcessFrame()
        {
            BenzinGrabTimeOnScopeExit(m_Timings[benzin::ApplicationTiming::ProcessFrame]);

            m_FrameTimerRef.Tick();

            m_FrameRateCounter.OnUpdate(m_FrameTimerRef.GetDeltaTime());

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
            BenzinGrabTimeOnScopeExit(m_Timings[benzin::ApplicationTiming::EndFrame]);

            m_Device->GetGraphicsCommandQueue().ExecuteCommandList();
            m_SwapChain->Flip();
        }

    private:
        bool OnWindowClose(benzin::WindowCloseEvent& event)
        {
            RequestShutdow();
            return false;
        };

        bool OnWindowResized(benzin::WindowResizedEvent& event)
        {
            m_Device->GetGraphicsCommandQueue().ExecuteCommandList(); // #TODO: Remove. Refactor in SwapChain::FlushAndResetBackBuffers
            m_SwapChain->ResizeBackBuffers(event.GetWidth(), event.GetHeight());
            return false;
        };

        bool OnKeyPressed(benzin::KeyPressedEvent& event)
        {
            if (event.GetKeyCode() == benzin::KeyCode::Escape)
            {
                RequestShutdow();
            }

            return false;
        }

        void RequestShutdow()
        {
            m_IsRunning = false;
        }

    public:
        std::unique_ptr<benzin::Window> m_MainWindow;
        std::unique_ptr<benzin::Backend> m_Backend;
        std::unique_ptr<benzin::Device> m_Device;
        std::unique_ptr<benzin::SwapChain> m_SwapChain;

        benzin::TickTimer& m_FrameTimerRef;
        bool& m_IsUpdateStatsIntervalPassedRef;

        benzin::LayerStack m_LayerStack;

        std::shared_ptr<benzin::ImGuiLayer> m_ImGuiLayer;
        std::shared_ptr<SceneLayer> m_SceneLayer;
        std::shared_ptr<RTHelloTriangleLayer> m_RTHelloTriangleLayer;
        std::shared_ptr<RTProceduralGeometryLayer> m_RTProceduralGeometryLayer;

        bool m_IsRunning = false;

        FrameRateCounter m_FrameRateCounter;
        benzin::ApplicationTimings m_Timings;
    };

} // namespace sandbox

int benzin::ClientMain()
{
    BenzinAssert(benzin::CommandLineArgsInstance::IsInitialized());
    const auto& commandLines = benzin::CommandLineArgsInstance::Get();

    benzin::GraphicsSettingsInstance::Initialize(benzin::GraphicsSettings
    {
        .MainAdapterIndex = commandLines.GetAdapterIndex(),
        .FrameInFlightCount = commandLines.GetFrameInFlightCount(),
        .DebugLayerParams
        {
            .IsGPUBasedValidationEnabled = commandLines.IsEnabledGPUBasedValidation(),
        },
    });

    {
        sandbox::Application application;
        application.ExecuteMainLoop();
    }

    return 0;
}
