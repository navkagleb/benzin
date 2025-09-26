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

        benzin::MakeUniquePtr(m_ImGuiManager, *m_MainWindow, m_FrameTimer);
        m_ImGuiManager->RegisterTool<benzin::FlyCameraTool>(m_CameraController);
        m_ImGuiManager->RegisterTool<benzin::GpuInfoTool>(*m_Backend);
        m_ImGuiManager->RegisterTool<benzin::GpuPrintTool>();
        m_ImGuiManager->RegisterTool<benzin::GpuProfilerTool>(*m_GpuProfiler);
        m_ImGuiManager->RegisterTool<benzin::PerformanceOverlayTool>(*m_MainWindow, *m_Backend, *m_Device, *m_ShaderManager, m_Viewport);
        m_ImGuiManager->RegisterTool<benzin::ProfilerTool>();
        m_ImGuiManager->RegisterTool<benzin::SceneStatsTool>(*m_Scene, *m_RayTracingScene);
        m_ImGuiManager->RegisterTool<benzin::SceneTool>(*m_Scene);
        m_ImGuiManager->RegisterTool<benzin::VramTool>(*m_Device);
        m_ImGuiManager->RegisterTool<benzin::TextureViewerTool>(m_TextureViewerData, m_Viewport, *m_RenderResources);
        m_ImGuiManager->RegisterTool<benzin::RenderViewportTool>(m_Viewport, *m_RenderResources);

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
            *m_RayTracingScene
        );

        benzin::ScopedGpuEvent::SetContext(*m_Device);
        benzin::ScopedGpuProfileEvent::SetContext(*m_Device, *m_GpuProfiler);

        m_CameraController.SetCamera(m_Scene->GetCamera());
    }

    Runner::~Runner()
    {
        BenzinLogTimeOnScopeExit("Runner::~Runner");

        m_Device->GetGraphicsCmdQueue().Flush();

        m_ImGuiManager->UnregisterTool<benzin::FlyCameraTool>();
        m_ImGuiManager->UnregisterTool<benzin::GpuInfoTool>();
        m_ImGuiManager->UnregisterTool<benzin::GpuPrintTool>();
        m_ImGuiManager->UnregisterTool<benzin::GpuProfilerTool>();
        m_ImGuiManager->UnregisterTool<benzin::PerformanceOverlayTool>();
        m_ImGuiManager->UnregisterTool<benzin::ProfilerTool>();
        m_ImGuiManager->UnregisterTool<benzin::SceneStatsTool>();
        m_ImGuiManager->UnregisterTool<benzin::SceneTool>();
        m_ImGuiManager->UnregisterTool<benzin::VramTool>();
        m_ImGuiManager->UnregisterTool<benzin::TextureViewerTool>();
        m_ImGuiManager->UnregisterTool<benzin::RenderViewportTool>();
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
        m_RenderPasses.push_back(std::make_unique<benzin::TextureViewerPass>(m_TextureViewerData));
        m_RenderPasses.push_back(std::make_unique<benzin::ImGuiPass>(*m_ImGuiManager));

        InitTools();
        InitScene();

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

            // TODO: Allocate GpuHeap with estimated size of meshes and provide it to the
            // scene (or create in the scene inself) and upload meshes to GPU using
            // linear allocator
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

            if (m_Viewport.IsHovered())
            {
                dispatcher.Dispatch<benzin::MouseMovedEvent>([&](const auto& event)
                {
                    if (!benzin::Input::IsMouseButtonPressed(benzin::MouseButton::Right))
                    {
                        benzin::Input::UnlockCursor();
                        return true;
                    }

                    const DirectX::XMINT2 mousePosition = event.GetPosition();
                    const DirectX::XMINT2 lockedCursorPosition = benzin::Input::LockCursor(*m_MainWindow);

                    m_CameraController.RotateCamera(mousePosition, lockedCursorPosition);

                    return true;
                });

                dispatcher.Dispatch<benzin::MouseScrolledEvent>([&](const auto& event)
                {
                    m_CameraController.IncrementFov((float)event.GetOffsetX());

                    return true;
                });
            }
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
        m_GpuProfiler->EndFrame();
        m_Device->GetGraphicsCmdQueue().SubmitCmdList();

        const bool isResized = m_SwapChain->OnFlip(m_IsVsyncEnabled);
        if (isResized)
        {
            benzin::RenderPass::SetWindowViewport(m_SwapChain->GetWidth(), m_SwapChain->GetHeight());
            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnWindowResize();
            }

            BenzinTrace("Window is resized: {} x {}. CpuFrame: {}", m_SwapChain->GetWidth(), m_SwapChain->GetHeight(), m_Device->GetCpuFrameIndex());
        }

        if (m_Viewport.IsResized())
        {
            // NOTE: Viewport size is controlled by UI. So first update UI and then resize render passes

            benzin::RenderPass::SetRenderViewport(m_Viewport.GetWidth(), m_Viewport.GetHeight());
            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnRenderViewportResize();
            }

            BenzinTrace("Viewport is resized: {} x {}. CpuFrame: {}", m_Viewport.GetWidth(), m_Viewport.GetHeight(), m_Device->GetCpuFrameIndex());

            m_CameraController.OnRenderViewportResized(m_Viewport.GetWidth(), m_Viewport.GetHeight());
        }

        m_Device->ProcessDeferredReleaseQueues();
    }

    void Runner::OnUpdate()
    {
        BenzinProfile();

        if (m_FrameTimer.IsPaused())
            return;

        m_CameraController.MoveCamera(m_FrameTimer.GetDeltaTime());

        {
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
        BenzinGpuProfile("Frame");

        for (auto& renderPass : m_RenderPasses)
        {
            const bool isViewportTestFailed = !m_Viewport.IsValidForRendering() && renderPass->IsDependentOnViewport();
            if (isViewportTestFailed || !renderPass->IsRenderingEnabled())
                continue;

            renderPass->OnRender();
        }

        m_GpuProfiler->ResolveTimestamps(m_Device->GetGraphicsCmdQueue().GetCmdList());
    }

    void Runner::RunImGuiFrame()
    {
        BenzinProfile();

        m_ImGuiManager->BeginUiFrame();
        m_ImGuiManager->DrawUi();
        m_ImGuiManager->EndUiFrame();
    }

}
