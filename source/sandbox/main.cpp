#include "bootstrap.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/command_line_args.hpp>
#include <benzin/core/entry_point.hpp>
#include <benzin/core/imgui_layer.hpp>
#include <benzin/core/layer_stack.hpp>
#include <benzin/core/logger.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/system/key_event.hpp>
#include <benzin/system/window.hpp>
#include <benzin/utility/debug_utils.hpp>

#include "scene_layer.hpp"
#include "rt_hello_triangle_layer.hpp"
#include "rt_procedural_geometry_layer.hpp"

namespace sandbox
{

    class FrameRateCounter
    {
    public:
        auto GetFrameRate() const { return m_FrameRate; }
        auto GetDeltaTime() const { return benzin::SecToUs(1.0f / m_FrameRate); }

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
            m_ElapsedTime += dt;
            m_ElapsedFrameCount++;
        }

    private:
        float m_FrameRate = 0.0f;

        std::chrono::microseconds m_ElapsedTime = std::chrono::microseconds::zero();
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

            const benzin::WindowCreation windowCreation
            {
                .Title = "Benzin: Sandbox",
                .Width = benzin::CommandLineArgs::GetWindowWidth(),
                .Height = benzin::CommandLineArgs::GetWindowHeight(),
                .IsResizable = benzin::CommandLineArgs::IsWindowResizable(),
                .EventCallback = [&](benzin::Event& event) { WindowEventCallback(event); },
            };

            benzin::MakeUniquePtr(m_MainWindow, windowCreation);
            benzin::MakeUniquePtr(m_Backend);
            benzin::MakeUniquePtr(m_Device, *m_Backend);
            benzin::MakeUniquePtr(m_SwapChain, *m_MainWindow, *m_Backend, *m_Device);

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
                m_SceneLayer = m_LayerStack.Push<SceneLayer>(graphicsRefs);
            }
            EndFrame();
        }

        ~Application()
        {
            BenzinLogTimeOnScopeExit("Shutdown Application");

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

                    m_ImGuiLayer->SetFrameRateStats(m_FrameRateCounter.GetFrameRate(), benzin::ToFloatMs(m_FrameRateCounter.GetDeltaTime()));
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
                layer->OnUpdate();
                layer->OnRender();
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
                m_ImGuiLayer->End(m_IsImGuiLayerClearsBackBuffer);
            }
        }

        void EndFrame()
        {
            BenzinGrabTimeOnScopeExit(m_Timings[benzin::ApplicationTiming::EndFrame]);

            // 'SwapChain::OnFlip' can update viewport dimenions, so if statemend below will be invalid
            const auto oldSwapChainWidth = (uint32_t)m_SwapChain->GetViewport().Width;
            const auto oldSwapChainHeight = (uint32_t)m_SwapChain->GetViewport().Height;

            m_Device->GetGraphicsCommandQueue().SumbitCommandList();
            m_SwapChain->OnFlip(m_IsVerticalSyncEnabled);

            if (m_PendingWidth != 0 && m_PendingHeight != 0 && (m_PendingWidth != oldSwapChainWidth || m_PendingHeight != oldSwapChainHeight))
            {
                for (auto& layer : m_LayerStack)
                {
                    layer->OnResize(m_PendingWidth, m_PendingHeight);
                }

                m_PendingWidth = 0;
                m_PendingHeight = 0;
            }
        }

    private:
        bool OnWindowClose(benzin::WindowCloseEvent& event)
        {
            RequestShutdown();
            return false;
        };

        bool OnWindowResized(benzin::WindowResizedEvent& event)
        {
            m_PendingWidth = event.GetWidth();
            m_PendingHeight = event.GetHeight();

            m_SwapChain->RequestResize(m_PendingWidth, m_PendingHeight);

            return false;
        };

        bool OnKeyPressed(benzin::KeyPressedEvent& event)
        {
            switch (event.GetKeyCode())
            {
                case benzin::KeyCode::Escape:
                {
                    RequestShutdown();
                    break;
                }
                case benzin::KeyCode::V:
                {
                    m_IsVerticalSyncEnabled = !m_IsVerticalSyncEnabled;
                    break;
                }
                case benzin::KeyCode::B:
                {
                    m_IsImGuiLayerClearsBackBuffer = !m_IsImGuiLayerClearsBackBuffer;
                    break;
                }
            }

            return false;
        }

        void RequestShutdown()
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
        benzin::ImGuiLayer* m_ImGuiLayer = nullptr;
        SceneLayer* m_SceneLayer = nullptr;

        bool m_IsRunning = false;
        bool m_IsVerticalSyncEnabled = true;
        bool m_IsImGuiLayerClearsBackBuffer = false;

        uint32_t m_PendingWidth = 0;
        uint32_t m_PendingHeight = 0;

        FrameRateCounter m_FrameRateCounter;
        benzin::ApplicationTimings m_Timings;
    };

} // namespace sandbox

int benzin::ClientMain()
{
#if 1
    {
        sandbox::Application application;
        application.ExecuteMainLoop();
    }
#else

    enum class BufferBindFlags
    {
        None = (0 << 0),  ///< None
        VertexBuffer = (1 << 0),  ///< Can bind as a vertex buffer
        IndexBuffer = (1 << 1),  ///< Can bind as an index buffer
        ConstantBuffer = (1 << 2),  ///< Can bind as a constant buffer.
        StructuredBuffer = (1 << 3),  ///< Can bind as a structured buffer.
        ByteAddressBuffer = (1 << 4),  ///< Can bind as a byte address buffer.
        FormattedBuffer = (1 << 5),  ///< Can bind as a formatted buffer.
        CopySrc = (1 << 6),  ///< Can bind as a copy source.
        CopyDst = (1 << 7),  ///< Can bind as a copy destination.
        ShaderReadWrite = (1 << 8),  ///< Can bind for usage as ReadWrite buffer for any shader stage (UAV on D3D12).
        PredicationExt = (1 << 9), ///< Can use the buffer for SetPredicationEx calls (must have AdapterFeatures::Predication requested feature enabled)
        AccelerationStructureExt = (1 << 10)  ///< Can bind as a ray tracing acceleration structure (require AdapterFeatures::RayTracing)
    };

    enum class AccessFlags
    {
        None             = (0 << 0),            ///< None
        CPURead          = (1 << 0),            ///< CPU read access allowed.  Cannot be set as input or outputs to the pipeline.
        CPUWrite         = (1 << 1),            ///< CPU write access allowed.  Cannot be set as outputs to the pipeline.
        CPUReadWrite     = CPURead | CPUWrite,  ///< CPU read write access allowed.
        GPURead          = (1 << 2),            ///< GPU read access allowed.
        GPUWrite         = (1 << 3),            ///< GPU write access allowed.
        GPUReadWrite     = GPURead | GPUWrite,  ///< GPU read write access allowed.
        GPUExecute       = (1 << 4),            ///< GPU execute access allowed.  Needed for indirect or counter buffer.
        GPULocal         = (1 << 5),            ///< GPU local memory. Needed to target CPU-visible VRAM
        CPUWriteCombined = (1 << 6),            ///< CPU write combined memory.
        IORead           = (1 << 7),            ///< IO read access allowed (PS5 only).
        IOWrite          = (1 << 8),            ///< IO write access allowed (PS5 only).
        MaxAccessFlags   = (1 << 9),            ///< Max value for access flags.
    };

    benzin::PrintEnumFlags(AccessFlags{ 100 });
    benzin::PrintEnumFlags(AccessFlags{ 68 });

    uint32_t errorCode = 0;

    *((volatile uint32_t*)2) = errorCode;

#endif
    return 0;
}
