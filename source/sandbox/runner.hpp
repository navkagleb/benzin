#pragma once

#include <benzin/engine/camera.hpp>
#include <benzin/engine/ray_tracing_scene.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics2/gpu_print_pass.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/imgui_pass.hpp>
#include <benzin/graphics2/pso_manager.hpp>
#include <benzin/graphics2/render_pass.hpp>
#include <benzin/graphics2/shader_manager.hpp>
#include <benzin/system/window.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>

namespace sandbox
{

    class Runner
    {
    public:
        Runner();
        virtual ~Runner();

        void RunMainLoop();

    protected:
        virtual void InitRenderPasses() = 0;
        virtual void InitTools() = 0;
        virtual void InitScene() = 0;

    private:
        void RunZeroFrame();

        void WindowEventCallback(benzin::Event& event);

        void BeginFrame();
        void EndFrame();

        void OnUpdate();
        void OnRender();

        void RunImGuiFrame();
        void ProcessResize();

    private:
        benzin::Window m_MainWindow;
        benzin::Backend m_Backend;
        benzin::Device m_Device;
        benzin::SwapChain m_SwapChain;

        benzin::ShaderManager m_ShaderManager;
        benzin::GpuProfiler m_GpuProfiler;
        benzin::PsoManager m_PsoManager;
        benzin::RenderResources m_RenderResources;

        benzin::GpuPrintData m_GpuPrintData;
        benzin::TextureViewerData m_TextureViewerData;
        benzin::RenderViewport m_Viewport;

        bool m_IsRunning = true;
        bool m_IsVsyncEnabled = false;
        bool m_IsSwapChainPendingResize = false;
        bool m_IsAnimationEnabled = false;

    protected:
        benzin::ImGuiManager m_ImGuiManager;

        benzin::RenderSettings m_RenderSettings;
        std::vector<std::unique_ptr<benzin::RenderPass>> m_RenderPasses;

        benzin::TickTimer m_FrameTimer;
        benzin::TickTimer m_AnimationTimer;

        benzin::Scene m_Scene;
        benzin::RayTracingScene m_RayTracingScene;
        benzin::FlyCameraController m_CameraController;

        using UpdateCallback = std::function<void()>;
        std::vector<UpdateCallback> m_UpdateCallbacks;
    };

}
