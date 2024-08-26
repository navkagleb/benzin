#include "sandbox/bootstrap.hpp"
#include "sandbox/runner.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/command_line_args.hpp>
#include <benzin/core/logger.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/imgui_pass.hpp>
#include <benzin/engine/render_pass.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/system/key_event.hpp>
#include <benzin/system/window.hpp>
#include <benzin/tools/fly_camera_tool.hpp>
#include <benzin/tools/performance_overlay_tool.hpp>
#include <benzin/tools/render_settings_tool.hpp>
#include <benzin/tools/render_viewport_tool.hpp>
#include <benzin/utility/time_utils.hpp>

#include "sandbox/tools/scene_stats_tool.hpp"
#include "sandbox/tools/tick_timer_tool.hpp"

namespace sandbox
{

    Runner::Runner()
        : m_1SecIntervalTimer{ std::chrono::seconds{ 1 } }
    {
        BenzinLogTimeOnScopeExit("Runner::Runner");

        benzin::MakeUniquePtr(m_MainWindow, benzin::WindowCreation
        {
            .Title = "benzin::SandboxRunner",
            .Width = benzin::CommandLineArgs::GetU32("WindowWidth"),
            .Height = benzin::CommandLineArgs::GetU32("WindowHeight"),
            .IsResizable = benzin::CommandLineArgs::GetBool("IsWindowResizable"),
            .EventCallback = [this](benzin::Event& event) { WindowEventCallback(event); },
        });

        benzin::MakeUniquePtr(m_Backend);
        benzin::MakeUniquePtr(m_Device, benzin::DeviceCreation{ "MainDevice", *m_Backend });
        benzin::MakeUniquePtr(m_SwapChain, benzin::SwapChainCreation{ "MainSwapChain", *m_MainWindow, *m_Device });

        benzin::MakeUniquePtr(m_Scene, *m_Device);
        benzin::MakeUniquePtr(m_FlyCameraController, m_Scene->GetCamera());

        benzin::MakeUniquePtr(m_RenderResources, *m_Device);
        benzin::MakeUniquePtr(m_RenderSettings);
        benzin::RenderPass::SetContext(*m_Device, *m_SwapChain, *m_RenderResources, *m_RenderSettings);

        benzin::MakeUniquePtr(m_ImGuiManager, *m_MainWindow, *m_Device);
        m_RenderViewportTool = m_ImGuiManager->PushTool<benzin::RenderViewportTool>(*m_RenderResources);
        m_RenderSettingsTool = m_ImGuiManager->PushTool<benzin::RenderSettingsTool>(*m_RenderSettings);
        m_PerformanceOverlayTool = m_ImGuiManager->PushTool<benzin::PerformanceOverlayTool>(*m_MainWindow, *m_Device, *m_SwapChain, *m_RenderViewportTool);
        m_ImGuiManager->PushTool<benzin::FlyCameraTool>(*m_FlyCameraController);
        m_ImGuiManager->PushTool<SceneStatsTool>(*m_Scene);
        m_ImGuiManager->PushTool<TickTimerTool>(m_FrameTimer);

        m_ImGuiManager->PushSpawnImGuiMenuCallback([this]
        {
            if (ImGui::BeginMenu("Runner"))
            {
                if (ImGui::MenuItem("VerticalSync", "V", m_IsVerticalSyncEnabled))
                {
                    ToggleVerticalSync();
                }

                if (ImGui::MenuItem("Animation", "F2", m_AnimationTimer.IsPaused()))
                {
                    ToggleAnimation();
                }

                ImGui::EndMenu();
            }
        });

        m_1SecIntervalTimer.PushCallback([this]
        {
            m_FpsCounter.UpdateFps(m_1SecIntervalTimer.GetInterval());
            m_PerformanceOverlayTool->SetFrameRateStats(m_FpsCounter.GetFps(), benzin::ToFloatMs(m_FpsCounter.GetDeltaTime()));
        });
    }

    Runner::~Runner()
    {
        BenzinTrace("----------------------------------------------");
        BenzinLogTimeOnScopeExit("Runner::~Runner");

        m_Device->GetGraphicsCommandQueue().Flush();
    }

    void Runner::RunMainLoop()
    {
        BenzinEnsure(m_IsRunning);
        BenzinEnsure(m_ImGuiPass != nullptr);

        RunZeroFrame();

        m_MainWindow->SetVisible(true);

        m_FrameTimer.Reset();
        m_AnimationTimer.Reset();

        while (m_IsRunning)
        {
            m_FrameTimer.Tick();
            m_AnimationTimer.Tick();
            m_1SecIntervalTimer.AccumulateInterval(m_FrameTimer);

            m_MainWindow->ProcessEvents();

            BeginFrame();
            {
                OnUpdate();
                OnRender();
            }
            EndFrame();
        }
    }

    void Runner::RunZeroFrame()
    {
        BenzinLogTimeOnScopeExit("Runner::RunZeroFrame");

        // Force call window resize on render passes
        benzin::RenderPass::SetWindowViewport(m_SwapChain->GetWidth(), m_SwapChain->GetHeight());
        for (auto& renderPass : m_RenderPasses)
        {
            renderPass->OnWindowResize(m_SwapChain->GetWidth(), m_SwapChain->GetHeight());
        }

        BeginFrame();
        {
            OnUpdate(); // Updates the UI. On EndFrame calls RenderPass::OnRenderViewportResize

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
        const benzin::EventDispatcher dispatcher{ event };
        {
            dispatcher.Dispatch<benzin::WindowCloseEvent>([this]
            {
                RequestShutdown();
                return false;
            });

            dispatcher.Dispatch<benzin::WindowEnterResizingEvent>([this]
            {
                m_FrameTimer.SetPaused(true);
                m_AnimationTimer.SetPaused(true);

                return false;
            });

            dispatcher.Dispatch<benzin::WindowExitResizingEvent>([this]
            {
                m_FrameTimer.SetPaused(false);
                m_AnimationTimer.SetPaused(false);

                return false;
            });

            dispatcher.Dispatch<benzin::WindowResizedEvent>([&](const auto& event)
            {
                m_SwapChain->RequestResize(event.GetWidth(), event.GetHeight());
                return false;
            });

            dispatcher.Dispatch<benzin::KeyPressedEvent>([&](const auto& event)
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
                        ToggleVerticalSync();
                        break;
                    }
                    case benzin::KeyCode::F2:
                    {
                        ToggleAnimation();
                        break;
                    }
                }

                return false;
            });
        }

        m_ImGuiManager->OnEvent(event);
        m_FlyCameraController->OnEvent(event);
    }

    void Runner::BeginFrame()
    {
        BenzinGrabTimeOnScopeExit(m_RunnerTimings[+RunnerTiming::BeginFrame]);

        m_Device->GetGraphicsCommandQueue().ResetCommandList();
    }

    void Runner::EndFrame()
    {
        BenzinGrabTimeOnScopeExit(m_RunnerTimings[+RunnerTiming::EndFrame]);

        m_Device->GetGpuTimer().ResolveTimestamps(m_Device->GetCpuFrameIndex());
        m_Device->GetGraphicsCommandQueue().SubmitCommandList();
        
        const bool isResized = m_SwapChain->OnFlip(m_IsVerticalSyncEnabled);
        if (isResized)
        {
            const auto windowWidth = m_SwapChain->GetWidth();
            const auto windowHeight = m_SwapChain->GetHeight();

            benzin::RenderPass::SetWindowViewport(windowWidth, windowHeight);
            for (auto& renderPass : m_RenderPasses)
            {
                // TODO: Remove windowWidth and windowHeight params
                renderPass->OnWindowResize(windowWidth, windowHeight);
            }
        }

        if (!m_RenderViewportTool->IsViewportSizeRelevant())
        {
            // Viewport size is controlled by UI. So first update UI and then resize render passes

            const auto viewportWidth = m_RenderViewportTool->GetWidth();
            const auto viewportHeight = m_RenderViewportTool->GetHeight();

            m_FlyCameraController->OnRenderViewportResized(viewportWidth, viewportHeight);

            benzin::RenderPass::SetRenderViewport(viewportWidth, viewportHeight);
            for (auto& renderPass : m_RenderPasses)
            {
                // TODO: Remove viewportWidth and viewportHeight params
                renderPass->OnRenderViewportResize(viewportWidth, viewportHeight);
            }
        }

        m_Device->GetPipelineStateManager().DestroyPendingPipelineStates();
        m_Device->GetPipelineStateManager().ReloadPipelineStatesIfNeeded();
        m_Device->ProcessDeferredReleaseQueues();
    }

    void Runner::OnUpdate()
    {
        BenzinGrabTimeOnScopeExit(m_RunnerTimings[+RunnerTiming::OnUpdate]);

        if (m_FrameTimer.IsPaused())
        {
            return;
        }

        m_FpsCounter.TickFrame(m_FrameTimer);

        m_FlyCameraController->OnUpdate(m_AnimationTimer.GetDeltaTime());
        m_Scene->OnUpdate();

        {
            m_ImGuiManager->BeginUiFrame();
            m_ImGuiManager->SpawnUi();
            m_ImGuiManager->EndUiFrame();
        }

        m_RenderResources->FlipResources();
        for (auto& renderPass : m_RenderPasses)
        {
            renderPass->OnUpdate(m_FrameTimer);
        }
    }

    void Runner::OnRender()
    {
        BenzinGrabTimeOnScopeExit(m_RunnerTimings[+RunnerTiming::OnRender]);
        BenzinPushGpuEvent(m_Device->GetGraphicsCommandQueue().GetCommandList(), "RenderPasses");

        for (auto& renderPass : m_RenderPasses)
        {
            const bool isViewportTestFailed = !m_RenderViewportTool->IsValidForRendering() && renderPass->IsDependentOnViewport();
            if (isViewportTestFailed || !renderPass->IsRenderingEnabled())
            {
                continue;
            }

            renderPass->OnRender();
        }
    }

    void Runner::RequestShutdown()
    {
        m_IsRunning = false;
    }

    void Runner::ToggleVerticalSync()
    {
        benzin::ToggleBool(m_IsVerticalSyncEnabled);
    }

    void Runner::ToggleAnimation()
    {
        m_AnimationTimer.SetPaused(!m_AnimationTimer.IsPaused());
    }

}
