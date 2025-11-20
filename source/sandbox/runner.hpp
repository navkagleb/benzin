#pragma once

#include <benzin/engine/camera.hpp>
#include <benzin/graphics2/gpu_print_pass.hpp>
#include <benzin/graphics2/render_pass.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>

namespace benzin
{
    class Backend;
    class GpuProfiler;
    class ShaderManager;
}

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

        void HandleSwapChainResizeIfNeeded();
        void HandleViewportResizeIfNeeded();

    protected:
        std::unique_ptr<benzin::Window> m_MainWindow;
        std::unique_ptr<benzin::Backend> m_Backend;
        std::unique_ptr<benzin::Device> m_Device;
        std::unique_ptr<benzin::SwapChain> m_SwapChain;

        std::unique_ptr<benzin::ShaderManager> m_ShaderManager;
        std::unique_ptr<benzin::GpuProfiler> m_GpuProfiler;
        std::unique_ptr<benzin::PsoManager> m_PsoManager;

        benzin::TickTimer m_FrameTimer;
        benzin::TickTimer m_AnimationTimer;

        std::unique_ptr<benzin::Scene> m_Scene;
        std::unique_ptr<benzin::RayTracing_Scene> m_RayTracingScene;

        std::unique_ptr<benzin::RenderResources> m_RenderResources;
        std::unique_ptr<benzin::RenderSettings> m_RenderSettings;
        std::vector<std::unique_ptr<benzin::RenderPass>> m_RenderPasses;

        std::unique_ptr<benzin::ImGuiManager> m_ImGuiManager;

        benzin::GpuPrintData m_GpuPrintData;
        benzin::TextureViewerData m_TextureViewerData;
        benzin::RenderViewport m_Viewport;
        benzin::FlyCameraController m_CameraController;

        using UpdateCallback = std::function<void()>;
        std::vector<UpdateCallback> m_UpdateCallbacks;

        bool m_IsRunning = true;
        bool m_IsVsyncEnabled = false;
        bool m_IsPendingResize = false;
        bool m_IsAnimationEnabled = false;
    };

}
