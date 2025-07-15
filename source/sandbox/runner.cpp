#include <sandbox/bootstrap.hpp>
#include <sandbox/runner.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/logger.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/ray_tracing_scene.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/gpu_profiler_pass.hpp>
#include <benzin/graphics2/imgui_pass.hpp>
#include <benzin/graphics2/pso_manager.hpp>
#include <benzin/graphics2/shader_manager.hpp>
#include <benzin/graphics2/texture_viewer_pass.hpp>
#include <benzin/system/input.hpp>
#include <benzin/system/key_event.hpp>
#include <benzin/system/window.hpp>
#include <benzin/tools/fly_camera_tool.hpp>
#include <benzin/tools/gpu_info_tool.hpp>
#include <benzin/tools/performance_overlay_tool.hpp>
#include <benzin/tools/profiler_tools.hpp>
#include <benzin/tools/render_viewport_tool.hpp>
#include <benzin/tools/scene_stats_tool.hpp>
#include <benzin/tools/scene_tool.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>

namespace sandbox
{

    Runner::Runner()
        : m_1SecIntervalTimer{ std::chrono::seconds{ 1 } }
    {
        BenzinLogTimeOnScopeExit("Runner::Runner");

        benzin::MakeUniquePtr(m_MainWindow, benzin::WindowCreation
        {
            .Title = "benzin::SandboxRunner",
            .Width = benzin::CmdLineArgs::GetWindowWidth(),
            .Height = benzin::CmdLineArgs::GetWindowHeight(),
            .IsResizable = benzin::CmdLineArgs::IsWindowResizable(),
        });
        m_MainWindow->SetEventCallback([this](benzin::Event& event) { WindowEventCallback(event); });

        benzin::MakeUniquePtr(m_Backend);
        benzin::MakeUniquePtr(m_Device, benzin::DeviceCreation{ "MainDevice", *m_Backend });
        benzin::MakeUniquePtr(m_SwapChain, benzin::SwapChainCreation{ "MainSwapChain", *m_MainWindow, *m_Backend, *m_Device });

        benzin::MakeUniquePtr(m_ShaderManager);
        benzin::MakeUniquePtr(m_GpuProfiler, *m_Device);
        benzin::MakeUniquePtr(m_PsoManager, *m_Device, *m_ShaderManager);
        benzin::MakeUniquePtr(m_ConstBufferPool, *m_Device);

        benzin::MakeUniquePtr(m_Scene, *m_Device);
        benzin::MakeUniquePtr(m_RayTracingScene, *m_Device, *m_Scene);

        benzin::MakeUniquePtr(m_RenderResources, *m_Device);
        benzin::MakeUniquePtr(m_RenderSettings);

        benzin::RenderPass::SetContext(
            *m_Device,
            *m_SwapChain,
            *m_GpuProfiler,
            *m_PsoManager,
            *m_ConstBufferPool,
            *m_RenderResources,
            *m_RenderSettings,
            m_FrameTimer,
            m_AnimationTimer,
            *m_Scene,
            *m_RayTracingScene
        );

        {
            benzin::MakeUniquePtr(m_ImGuiManager, *m_MainWindow, *m_Device, m_FrameTimer);

            m_TextureViewerTool = m_ImGuiManager->PushTool<benzin::TextureViewerTool>(*m_RenderResources);
            m_RenderViewportTool = m_ImGuiManager->PushTool<benzin::RenderViewportTool>(*m_RenderResources, *m_TextureViewerTool, m_Scene->GetCamera());
            m_PerformanceOverlayTool = m_ImGuiManager->PushTool<benzin::PerformanceOverlayTool>(*m_MainWindow, *m_Backend, *m_Device, *m_ShaderManager, *m_RenderViewportTool);

            m_ImGuiManager->PushTool<benzin::FlyCameraTool>(*m_RenderViewportTool);
            m_ImGuiManager->PushTool<benzin::GpuInfoTool>(*m_Backend);
            m_ImGuiManager->PushTool<benzin::GpuProfilerTool>(*m_GpuProfiler);
            m_ImGuiManager->PushTool<benzin::ProfilerTool>();
            m_ImGuiManager->PushTool<benzin::SceneStatsTool>(*m_Scene, *m_RayTracingScene);
            m_ImGuiManager->PushTool<benzin::SceneTool>(*m_Scene);

            m_ImGuiManager->AddDrawMenuCallback([this]
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
        }

        m_1SecIntervalTimer.PushCallback([this](uint32_t)
        {
            m_FpsCounter.UpdateFps(m_1SecIntervalTimer.GetInterval());
            m_PerformanceOverlayTool->SetFrameRateStats(m_FpsCounter.GetFps(), benzin::ToFloatMs(m_FpsCounter.GetDeltaTime()));
        });
    }

    Runner::~Runner()
    {
        BenzinTrace(benzin::Logger::GetLineSeparator());
        BenzinLogTimeOnScopeExit("Runner::~Runner");

        m_Device->GetGraphicsCmdQueue().Flush();
    }

    void Runner::RunMainLoop()
    {
        BenzinEnsure(m_IsRunning);

        RunZeroFrame();

        m_FrameTimer.Reset();
        m_AnimationTimer.Reset();
        m_AnimationTimer.SetPaused(true);

        while (m_IsRunning)
        {
            benzin::Profiler::BeginFrame();
            BenzinExecuteOnScopeExit([] { benzin::Profiler::EndFrame(); });

            BenzinScopeProfile("Frame");

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

        InitRenderPasses();
        InitTools();
        InitScene();

        {
            m_RenderPasses.push_back(std::make_unique<benzin::TextureViewerPass>(*m_TextureViewerTool));
            m_RenderPasses.push_back(std::make_unique<benzin::ImGuiPass>(*m_ImGuiManager));
            m_RenderPasses.push_back(std::make_unique<benzin::GpuProfilerPass>());
        }

        // Force call window resize on render passes
        benzin::RenderPass::SetWindowViewport(m_SwapChain->GetWidth(), m_SwapChain->GetHeight());
        for (auto& renderPass : m_RenderPasses)
        {
            renderPass->OnWindowResize();
        }

        BeginFrame();
        {
            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnZeroFrameInit();
            }

            m_Scene->UploadMeshesToGpu();
            m_Scene->UploadMeshletsToGpu();
            m_Scene->UploadMaterialsToGpu();
            m_RayTracingScene->BuildBlases();

            RunImGuiFrame(); // Force call ImGui frame to call RenderPass::OnRenderViewportResize on EndFrame
        }
        EndFrame();

        m_MainWindow->SetVisible(true);
    }

    void Runner::WindowEventCallback(benzin::Event& event)
    {
        const benzin::EventDispatcher dispatcher{ event };
        {
            dispatcher.Dispatch<benzin::WindowCloseEvent>([this]
            {
                RequestShutdown();
                return true;
            });

            dispatcher.Dispatch<benzin::WindowEnterResizingEvent>([this]
            {
                m_FrameTimer.SetPaused(true);

                if (m_IsAnimationEnabled)
                {
                    m_AnimationTimer.SetPaused(true);
                }

                return false;
            });

            dispatcher.Dispatch<benzin::WindowExitResizingEvent>([this]
            {
                m_FrameTimer.SetPaused(false);

                if (m_IsAnimationEnabled)
                {
                    m_AnimationTimer.SetPaused(false);
                }

                return false;
            });

            dispatcher.Dispatch<benzin::WindowResizedEvent>([&](const auto& event)
            {
                m_SwapChain->RequestResize(event.GetWidth(), event.GetHeight());
                return true;
            });

            dispatcher.Dispatch<benzin::KeyPressedEvent>([&](const auto& event)
            {
                switch (event.GetKeyCode())
                {
                    case benzin::KeyCode::Escape:
                    {
                        RequestShutdown();
                        return true;
                    }
                    case benzin::KeyCode::V:
                    {
                        ToggleVerticalSync();
                        return true;
                    }
                    case benzin::KeyCode::F2:
                    {
                        ToggleAnimation();
                        return true;
                    }
                }

                return false;
            });
        }

        m_ImGuiManager->OnEvent(event);

        benzin::Input::SetCursorPositionIfNeeded();
    }

    void Runner::BeginFrame()
    {
        BenzinProfile();
    
        m_Device->GetGraphicsCmdQueue().ResetCmdList();
        m_GpuProfiler->BeginFrame(m_Device->GetCpuFrameIndex());
        m_ConstBufferPool->BeginFrame();

        m_ShaderManager->CheckForNewShader();
        m_Device->ProcessDeferredReleaseQueues();
    }

    void Runner::EndFrame()
    {
        BenzinProfile();

        if (m_RenderViewportTool->IsValidForRendering())
        {
            // Check only if render viewport is valid for rendering. Because actual const buffer allocation apper in 'RenderPass::OnRender'
            m_ConstBufferPool->EndFrame();
        }

        m_Device->GetGraphicsCmdQueue().SubmitCmdList();

        const bool isResized = m_SwapChain->OnFlip(m_IsVerticalSyncEnabled);
        if (isResized)
        {
            const auto windowWidth = m_SwapChain->GetWidth();
            const auto windowHeight = m_SwapChain->GetHeight();

            benzin::RenderPass::SetWindowViewport(windowWidth, windowHeight);
            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnWindowResize();
            }

            BenzinTrace("Window is resized: {} x {}. CpuFrame: {}", windowWidth, windowHeight, m_Device->GetCpuFrameIndex());
        }

        if (!m_RenderViewportTool->IsViewportSizeValid())
        {
            // Viewport size is controlled by UI. So first update UI and then resize render passes

            const auto viewportWidth = m_RenderViewportTool->GetWidth();
            const auto viewportHeight = m_RenderViewportTool->GetHeight();

            benzin::RenderPass::SetRenderViewport(viewportWidth, viewportHeight);
            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnRenderViewportResize();
            }

            BenzinTrace("Viewport is resized: {} x {}. CpuFrame: {}", viewportWidth, viewportHeight, m_Device->GetCpuFrameIndex());
        }
    }

    void Runner::OnUpdate()
    {
        BenzinProfile();

        if (m_FrameTimer.IsPaused())
        {
            return;
        }

        m_FpsCounter.TickFrame(m_FrameTimer);

        m_RenderViewportTool->MoveCamera(m_FrameTimer.GetDeltaTime());

        {
            BenzinScopeProfile("Update scene");

            if (!m_AnimationTimer.IsPaused())
            {
                m_Scene->UpdateEntities();
            }

            m_Scene->UploadEntityTransformsToGpu();
            m_Scene->UploadLightsToGpu();
        }

        RunImGuiFrame();

        m_RenderResources->FlipResources();
        for (auto& renderPass : m_RenderPasses)
        {
            renderPass->OnUpdate();
        }

        m_ConstBufferPool->AllocatePools();
    }

    void Runner::OnRender()
    {
        BenzinProfile();

#if BENZIN_IS_GPU_PROFILER_ENABLED
        auto& cmdList = m_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*m_GpuProfiler, cmdList, "Frame");
#endif

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

    void Runner::RunImGuiFrame()
    {
        BenzinProfile();

        m_ImGuiManager->BeginUiFrame();
        m_ImGuiManager->DrawUi();
        m_ImGuiManager->EndUiFrame();
    }

    void Runner::RequestShutdown()
    {
        m_IsRunning = false;
    }

    void Runner::ToggleVerticalSync()
    {
        m_IsVerticalSyncEnabled = !m_IsVerticalSyncEnabled;
    }

    void Runner::ToggleAnimation()
    {
        m_AnimationTimer.SetPaused(!m_AnimationTimer.IsPaused());
        m_IsAnimationEnabled = !m_IsAnimationEnabled;
    }

}
