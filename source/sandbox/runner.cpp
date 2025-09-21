#include <sandbox/bootstrap.hpp>
#include <sandbox/runner.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/logger.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/ray_tracing_scene.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
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
#include <benzin/tools/vram_tool.hpp>

namespace sandbox
{

    Runner::Runner()
    {
        BenzinLogTimeOnScopeExit("Runner::Runner");

        benzin::MakeUniquePtr(m_MainWindow, benzin::WindowCreation
        {
            .Title = "Benzin Renderer",
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

        benzin::MakeUniquePtr(m_Scene, *m_Device);
        benzin::MakeUniquePtr(m_RayTracingScene, *m_Device, *m_Scene);

        benzin::MakeUniquePtr(m_RenderResources, *m_Device);
        benzin::MakeUniquePtr(m_RenderSettings);

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
            m_ImGuiManager->PushTool<benzin::VramTool>(*m_Device);
        }

        benzin::RenderPass::SetContext(
            *m_Device,
            *m_SwapChain,
            *m_GpuProfiler,
            *m_PsoManager,
            *m_RenderResources,
            *m_RenderSettings,
            m_FrameTimer,
            m_AnimationTimer,
            *m_Scene,
            *m_RayTracingScene,
        );
    }

    Runner::~Runner()
    {
        BenzinTrace(benzin::Logger::GetLineSeparator());
        BenzinLogTimeOnScopeExit("Runner::~Runner");

        m_Device->GetGraphicsCmdQueue().Flush();
    }

    void Runner::RunMainLoop()
    {
        RunZeroFrame();

        m_FrameTimer.Reset();
        m_AnimationTimer.Reset();
        m_AnimationTimer.SetPaused(!m_IsAnimationEnabled);

        while (m_IsRunning)
        {
            benzin::Profiler::BeginFrame();
            BenzinExecuteOnScopeExit([] { benzin::Profiler::EndFrame(); });

            BenzinScopeProfile("Frame");

            BeginFrame();
            OnUpdate();
            OnRender();
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
                m_IsRunning = false;
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
                        m_IsRunning = false;
                        return true;
                    }
                    case benzin::KeyCode::V:
                    {
                        m_IsVsyncEnabled = !m_IsVsyncEnabled;
                        return true;
                    }
                    case benzin::KeyCode::F2:
                    {
                        m_IsAnimationEnabled = !m_IsAnimationEnabled;
                        m_AnimationTimer.SetPaused(!m_IsAnimationEnabled);
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

        m_FrameTimer.Tick();
        m_AnimationTimer.Tick();

        m_MainWindow->ProcessEvents();

        m_Device->GetTemporalLinearAllocator().Reset();
        m_Device->GetConstBufferAllocator().ResetFrameBuffer();
        m_Device->GetGraphicsCmdQueue().ResetCmdList();

        m_ImGuiManager->BeginFrame();

        m_GpuProfiler->BeginFrame(m_Device->GetCpuFrameIndex());
        m_ShaderManager->CheckForNewShader();
    }

    void Runner::EndFrame()
    {
        BenzinProfile();

        m_Scene->EndFrame();
        m_Device->GetGraphicsCmdQueue().SubmitCmdList();

        const bool isResized = m_SwapChain->OnFlip(m_IsVsyncEnabled);
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

        if (m_RenderViewportTool->IsViewportResized())
        {
            // NOTE: Viewport size is controlled by UI. So first update UI and then resize render passes

            const auto viewportWidth = m_RenderViewportTool->GetWidth();
            const auto viewportHeight = m_RenderViewportTool->GetHeight();

            benzin::RenderPass::SetRenderViewport(viewportWidth, viewportHeight);
            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnRenderViewportResize();
            }

            BenzinTrace("Viewport is resized: {} x {}. CpuFrame: {}", viewportWidth, viewportHeight, m_Device->GetCpuFrameIndex());
        }

        m_Device->ProcessDeferredReleaseQueues();
    }

    void Runner::OnUpdate()
    {
        BenzinProfile();

        if (m_FrameTimer.IsPaused())
            return;

        m_RenderViewportTool->MoveCamera(m_FrameTimer.GetDeltaTime());

        {
            BenzinScopeProfile("Update scene");
            BenzinScopeProfile("Runner::<update scene>");

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
    }

    void Runner::OnRender()
    {
        BenzinProfile();

        BenzinGpuProfile(*m_GpuProfiler, m_Device->GetGraphicsCmdQueue().GetCmdList(), "Frame");

        for (auto& renderPass : m_RenderPasses)
        {
            const bool isViewportTestFailed = !m_RenderViewportTool->IsValidForRendering() && renderPass->IsDependentOnViewport();
            if (isViewportTestFailed || !renderPass->IsRenderingEnabled())
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

    void Runner::ToggleVsync()
}
