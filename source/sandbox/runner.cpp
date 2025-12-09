#include <sandbox/bootstrap.hpp>
#include <sandbox/runner.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/imgui_pass.hpp>
#include <benzin/graphics2/pso_manager.hpp>
#include <benzin/graphics2/shader_manager.hpp>
#include <benzin/graphics2/texture_viewer_pass.hpp>
#include <benzin/system/input.hpp>
#include <benzin/system/key_event.hpp>
#include <benzin/system/mouse_event.hpp>
#include <benzin/system/window.hpp>
#include <benzin/system/window_event.hpp>
#include <benzin/tools/fly_camera_tool.hpp>
#include <benzin/tools/gpu_info_tool.hpp>
#include <benzin/tools/performance_overlay_tool.hpp>
#include <benzin/tools/profiler_tools.hpp>
#include <benzin/tools/render_viewport_tool.hpp>

namespace sandbox
{

    Runner::Runner()
    {
        BenzinTraceScopeTime("Runner::Runner");

        benzin::WindowCreation windowCreation;
        windowCreation.m_Title = "Benzin Demo";
        windowCreation.m_Width = benzin::CmdLineArgs::GetWindowWidth();
        windowCreation.m_Height = benzin::CmdLineArgs::GetWindowHeight();

        benzin::MakeUniquePtr(m_MainWindow, windowCreation);
        m_MainWindow->SetEventCallback([this](benzin::Event& event) { WindowEventCallback(event); });

        benzin::MakeUniquePtr(m_Backend);
        benzin::MakeUniquePtr(m_Device, benzin::DeviceCreation{ "MainDevice", *m_Backend });
        benzin::MakeUniquePtr(m_SwapChain, benzin::SwapChainCreation{ "MainSwapChain", *m_MainWindow, *m_Backend, *m_Device });

        benzin::MakeUniquePtr(m_ShaderManager);
        benzin::MakeUniquePtr(m_GpuProfiler, *m_Device);
        benzin::MakeUniquePtr(m_PsoManager, *m_Device, *m_ShaderManager);

        benzin::MakeUniquePtr(m_RenderResources, m_Device->GetResDependentAllocator());
        benzin::MakeUniquePtr(m_RenderSettings);

        benzin::MakeUniquePtr(m_ImGuiManager, *m_MainWindow, m_FrameTimer);
        m_ImGuiManager->RegisterTool<benzin::FlyCameraTool>(m_CameraController);
        m_ImGuiManager->RegisterTool<benzin::GpuInfoTool>(*m_Backend);
        m_ImGuiManager->RegisterTool<benzin::GpuProfilerTool>(*m_GpuProfiler);
        m_ImGuiManager->RegisterTool<benzin::PerformanceOverlayTool>(*m_Backend, *m_Device, *m_ShaderManager, *m_GpuProfiler, m_Viewport, m_FrameTimer);
        m_ImGuiManager->RegisterTool<benzin::ProfilerTool>();
        m_ImGuiManager->RegisterTool<benzin::GpuPrintTool>(m_GpuPrintData);
        m_ImGuiManager->RegisterTool<benzin::TextureViewerTool>(m_TextureViewerData, m_Viewport, *m_RenderResources);
        m_ImGuiManager->RegisterTool<benzin::RenderViewportTool>(m_Viewport, *m_RenderResources);

        benzin::RenderPass::SetContext(
            *m_Device,
            *m_SwapChain,
            *m_PsoManager,
            *m_RenderResources,
            *m_RenderSettings,
            m_FrameTimer,
            m_AnimationTimer,
            m_Scene,
            m_RayTracingScene);

        benzin::ScopedGpuEvent::SetContext(*m_Device);
        benzin::ScopedGpuProfileEvent::SetContext(*m_Device, *m_GpuProfiler);

        m_CameraController.SetCamera(m_Scene.m_Camera);
    }

    Runner::~Runner()
    {
        BenzinTraceScopeTime("Runner::~Runner");

        m_ImGuiManager->UnregisterTool<benzin::FlyCameraTool>();
        m_ImGuiManager->UnregisterTool<benzin::GpuInfoTool>();
        m_ImGuiManager->UnregisterTool<benzin::GpuProfilerTool>();
        m_ImGuiManager->UnregisterTool<benzin::PerformanceOverlayTool>();
        m_ImGuiManager->UnregisterTool<benzin::ProfilerTool>();
        m_ImGuiManager->UnregisterTool<benzin::GpuPrintTool>();
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
        BenzinTraceScopeTime("Runner::RunZeroFrame");

        m_RenderPasses.push_back(std::make_unique<benzin::GpuPrintPass>(m_GpuPrintData));
        InitRenderPasses();
        m_RenderPasses.push_back(std::make_unique<benzin::TextureViewerPass>(m_TextureViewerData));
        m_RenderPasses.push_back(std::make_unique<benzin::ImGuiPass>(*m_ImGuiManager));

        InitTools();
        InitScene();

        // Force set window size for render passes
        benzin::RenderPass::SetWindowSize(m_MainWindow->GetWidth(), m_MainWindow->GetHeight());

        BeginFrame();
        {
            m_Scene.UploadMeshDrawsToGpu(*m_Device);
            m_Scene.UploadMeshGeometryToGpu(*m_Device);

            for (auto& renderPass : m_RenderPasses)
            {
                renderPass->OnZeroFrameInit();
            }

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

            dispatcher.Dispatch<benzin::WindowResizedEvent>([&]
            {
                m_IsPendingResize = true;
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

        m_Device->GetConstBufferAllocator().ResetFrameBuffer();
        m_Device->GetGraphicsCmdQueue().ResetCmdList();

        m_ImGuiManager->BeginFrame();

        m_GpuProfiler->BeginFrame(m_Device->GetCpuFrameIndex());
        m_ShaderManager->CheckForNewShader();
    }

    void Runner::EndFrame()
    {
        BenzinProfile();

        m_GpuProfiler->EndFrame();

        m_Device->GetGraphicsCmdQueue().SubmitCmdList(m_SwapChain->GetCurrentBackBuffer());
        m_Device->SignalFrameFence();

        m_SwapChain->Flip(m_IsVsyncEnabled);
        m_Device->WaitForGpuIfNeeded();
        m_Device->AdvanceFrame(m_SwapChain->GetCurrentBackBufferIndex());

        HandleSwapChainResizeIfNeeded();
        HandleViewportResizeIfNeeded();

        m_Device->ProcessDeferredReleaseQueues();

        m_RenderResources->FlipIndex();
    }

    void Runner::OnUpdate()
    {
        BenzinProfile();

        if (m_FrameTimer.IsPaused())
            return;

        m_CameraController.MoveCamera(m_FrameTimer.GetDeltaTimeInMs());

        if (!m_AnimationTimer.IsPaused())
        {
            for (const UpdateCallback& callback : m_UpdateCallbacks)
            {
                callback();
            }
        }

        m_Scene.UploadMeshDrawsToGpu(*m_Device);

        RunImGuiFrame();

        m_GpuPrintData.m_CursorPosition = m_Viewport.GetCursorPosition(); // Update it after ImGui frame is done

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

    void Runner::HandleSwapChainResizeIfNeeded()
    {
        if (!m_IsPendingResize)
            return;

        m_IsPendingResize = false;

        const uint32_t width = m_MainWindow->GetWidth();
        const uint32_t height = m_MainWindow->GetHeight();

        m_Device->GetGraphicsCmdQueue().Flush();
        m_Device->GetResDependentAllocator().ResetOffset();
        m_SwapChain->Resize(width, height);

        benzin::RenderPass::SetWindowSize(width, height);

        BenzinTrace("Swap chain resized. CpuFrame: {}", m_Device->GetCpuFrameIndex());
    }

    void Runner::HandleViewportResizeIfNeeded()
    {
        if (!m_Viewport.IsPendingResize())
            return;

        m_Device->GetGraphicsCmdQueue().Flush();
        m_Device->GetResDependentAllocator().ResetOffset();

        m_Viewport.Resize();

        const uint32_t width = m_Viewport.GetWidth();
        const uint32_t height = m_Viewport.GetHeight();

        m_CameraController.OnRenderViewportResized(width, height);

        benzin::RenderPass::SetRenderViewport(width, height);
        for (auto& renderPass : m_RenderPasses)
        {
            renderPass->OnRenderViewportResize();
        }

        BenzinTrace("Render viewport resized. CpuFrame: {}", m_Device->GetCpuFrameIndex());
    }

}
