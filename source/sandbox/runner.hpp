#pragma once

#include <benzin/core/interval_timer.hpp>
#include <benzin/core/tick_timer.hpp>

#include "sandbox/fps_counter.hpp"
#include "sandbox/tools/timings_tool.hpp"

namespace benzin
{

    class Backend;
    class Device;
    class Event;
    class ImGuiManager;
    class ImGuiPass;
    class PerformanceOverlayTool;
    class RayTracing_Scene;
    class RenderPass;
    class RenderResources;
    class RenderSettingsTool;
    class RenderViewportTool;
    class Scene;
    class SwapChain;
    class TextureViewerTool;
    class Window;

}

namespace sandbox
{

    class Runner
    {
    public:
        Runner();
        virtual ~Runner();

        void RunMainLoop();

    private:
        void RunZeroFrame();

        void WindowEventCallback(benzin::Event& event);

        void BeginFrame();
        void EndFrame();

        void OnUpdate();
        void OnRender();

        void RequestShutdown();
        void ToggleVerticalSync();
        void ToggleAnimation();

    protected:
        std::unique_ptr<benzin::Window> m_MainWindow;
        std::unique_ptr<benzin::Backend> m_Backend;
        std::unique_ptr<benzin::Device> m_Device;
        std::unique_ptr<benzin::SwapChain> m_SwapChain;

        benzin::TickTimer m_FrameTimer;
        benzin::TickTimer m_AnimationTimer;
        benzin::IntervalTimer m_1SecIntervalTimer;

        bool m_IsAnimationEnabled = true;

        FpsCounter m_FpsCounter;

        std::unique_ptr<benzin::Scene> m_Scene;
        std::unique_ptr<benzin::RayTracing_Scene> m_RayTracingScene;

        std::unique_ptr<benzin::RenderResources> m_RenderResources;
        std::unique_ptr<benzin::RenderSettings> m_RenderSettings;
        std::vector<std::unique_ptr<benzin::RenderPass>> m_RenderPasses;
        benzin::ImGuiPass* m_ImGuiPass = nullptr;

        std::unique_ptr<benzin::ImGuiManager> m_ImGuiManager;
        benzin::RenderViewportTool* m_RenderViewportTool = nullptr;
        benzin::RenderSettingsTool* m_RenderSettingsTool = nullptr;
        benzin::TextureViewerTool* m_TextureViewerTool = nullptr;

        RunnerTimings m_RunnerTimings{};

        bool m_IsRunning = true;
        bool m_IsVerticalSyncEnabled = true;

    private:
        benzin::PerformanceOverlayTool* m_PerformanceOverlayTool = nullptr;
    };

}
