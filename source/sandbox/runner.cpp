#include "sandbox/bootstrap.hpp"
#include "sandbox/runner.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/command_line_args.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/imgui_pass.hpp>
#include <benzin/engine/render_pass.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/system/key_event.hpp>
#include <benzin/system/window.hpp>
#include <benzin/tools/fly_camera_tool.hpp>
#include <benzin/utility/time_utils.hpp>

#include "sandbox/tools/scene_stats_tool.hpp"
#include "sandbox/tools/tick_timer_tool.hpp"

namespace sandbox
{

    Runner::Runner()
    {
        BenzinLogTimeOnScopeExit("Create Runner");

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
        benzin::MakeUniquePtr(m_Device, benzin::DeviceCreation{ "MainDevice", *m_Backend });
        benzin::MakeUniquePtr(m_SwapChain, benzin::SwapChainCreation{ "MainSwapChain", *m_MainWindow, *m_Backend, *m_Device });

        benzin::MakeUniquePtr(m_Scene, *m_Device);
        benzin::MakeUniquePtr(m_FlyCameraController, m_Scene->GetCamera());

        benzin::MakeUniquePtr(m_ImGuiManager, *m_MainWindow, *m_Device);

        m_BottomPanelTool = m_ImGuiManager->PushTool<BottomPanelTool>(*m_MainWindow, *m_Backend, *m_Device, *m_SwapChain);
        m_ImGuiManager->PushTool<benzin::FlyCameraTool>(*m_FlyCameraController);
        m_ImGuiManager->PushTool<SceneStatsTool>(*m_Scene);
        m_ImGuiManager->PushTool<TickTimerTool>(m_FrameTimer);
    }

    Runner::~Runner()
    {
        BenzinLogTimeOnScopeExit("Shutdown Runner");

        // TODO: Need to flush until deferred release is implemented
        m_Device->GetGraphicsCommandQueue().Flush();
    }

    void Runner::Client_OnEvent(benzin::Event& event)
    {
        BenzinUnused(event);
    }

    void Runner::Client_Init()
    {
        Client_InitTools();
        Client_InitRenderPasses();

        Client_InitSceneEntities();
    }

    void Runner::RunMainLoop()
    {
        BenzinEnsure(m_IsRunning);
        m_FrameTimer.Reset();

        RunZeroFrame();

        while (m_IsRunning)
        {
            m_MainWindow->ProcessEvents();

            BeginFrame();
            ProcessFrame();
            EndFrame();

            Client_AfterEndFrame();

            if (m_FrameRateCounter.IsIntervalPassed())
            {
                m_FrameRateCounter.UpdateFrameRate();

                m_BottomPanelTool->SetFrameRateStats(m_FrameRateCounter.GetFrameRate(), benzin::ToFloatMs(m_FrameRateCounter.GetDeltaTime()));
                m_BottomPanelTool->SetRunnerTimings(m_Timings);
            }
        }
    }

    void Runner::RunZeroFrame()
    {
        for (auto& renderPass : m_RenderPasses)
        {
            if (renderPass->IsRenderingEnabled())
            {
                renderPass->OnResize(m_SwapChain->GetViewportWidth(), m_SwapChain->GetViewportHeight());
            }
        }

        BeginFrame();
        {
            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnZeroFrameInit();
            }

            {
                BenzinLogTimeOnScopeExit("Upload scene data to GPU");
                m_Scene->UploadMeshCollections();
            }

            {
                BenzinLogTimeOnScopeExit("Build scene RT BottomLevel ASs");
                m_Scene->BuildBottomLevelAccelerationStructures();
            }
        }
        EndFrame();
    }

    void Runner::WindowEventCallback(benzin::Event& event)
    {
        benzin::EventDispatcher dispatcher{ event };
        {
            dispatcher.Dispatch<benzin::WindowCloseEvent>([&](auto& event)
            {
                BenzinUnused(event);

                RequestShutdown();
                return false;
            });

            dispatcher.Dispatch<benzin::WindowResizedEvent>([&](auto& event)
            {
                m_PendingWidth = event.GetWidth();
                m_PendingHeight = event.GetHeight();

                m_SwapChain->RequestResize(m_PendingWidth, m_PendingHeight);

                return false;
            });

            dispatcher.Dispatch<benzin::KeyPressedEvent>([&](auto& event)
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
                    case benzin::KeyCode::F1:
                    {
                        m_ImGuiPass->SetRenderingEnabled(!m_ImGuiPass->IsRenderingEnabled());
                        break;
                    }
                }

                return false;
            });
        }

        m_ImGuiManager->OnEvent(event);
        m_FlyCameraController->OnEvent(event);

        Client_OnEvent(event);
    }

    void Runner::BeginFrame()
    {
        BenzinGrabTimeOnScopeExit(m_Timings[+RunnerTiming::BeginFrame]);

        m_Device->GetGraphicsCommandQueue().OnFrameBegin();
    }

    void Runner::ProcessFrame()
    {
        m_FrameTimer.Tick();

        OnUpdate();
        OnRender();
    }

    void Runner::EndFrame()
    {
        BenzinGrabTimeOnScopeExit(m_Timings[+RunnerTiming::EndFrame]);

        // 'SwapChain::OnFlip' can update viewport dimenions, so if statemend below will be invalid
        const auto oldSwapChainWidth = m_SwapChain->GetViewportWidth();
        const auto oldSwapChainHeight = m_SwapChain->GetViewportHeight();

        m_Device->GetGpuTimer().ResolveTimestamps(m_Device->GetCpuFrameIndex());
        m_Device->GetGraphicsCommandQueue().OnFrameEnd();
        m_SwapChain->OnFlip(m_IsVerticalSyncEnabled);
        m_Device->ProcessDeferredReleaseQueues();

        if (m_PendingWidth != 0 && m_PendingHeight != 0 && (m_PendingWidth != oldSwapChainWidth || m_PendingHeight != oldSwapChainHeight))
        {
            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnResize(m_PendingWidth, m_PendingHeight);
            }

            m_PendingWidth = 0;
            m_PendingHeight = 0;
        }
    }

    void Runner::OnUpdate()
    {
        BenzinGrabTimeOnScopeExit(m_Timings[+RunnerTiming::OnUpdate]);

        m_FrameRateCounter.OnUpdate(m_FrameTimer);
        m_FlyCameraController->OnUpdate(m_FrameTimer.GetDeltaTime());
        m_Scene->OnUpdate(m_FrameTimer);

        Client_OnUpdate();

        m_RenderResources->FlipResources();
        for (auto& renderPass : m_RenderPasses)
        {
            renderPass->OnUpdate(m_FrameTimer);
        }
    }

    void Runner::OnRender()
    {
        BenzinGrabTimeOnScopeExit(m_Timings[+RunnerTiming::OnRender]);

        auto& commandList = m_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "RenderPasses");

        for (auto& renderPass : m_RenderPasses)
        {
            if (renderPass->IsRenderingEnabled())
            {
                renderPass->OnRender();
            }
        }
    }

    void Runner::RequestShutdown()
    {
        m_IsRunning = false;
    }

}
