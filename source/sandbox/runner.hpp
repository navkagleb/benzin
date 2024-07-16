#pragma once

#include <benzin/core/interval_timer.hpp>
#include <benzin/core/tick_timer.hpp>

#include "sandbox/fps_counter.hpp"
#include "sandbox/tools/bottom_panel_tool.hpp"

namespace benzin
{

    class Backend;
    class Device;
    class Event;
    class FlyCameraController;
    class ImGuiManager;
    class ImGuiPass;
    class RenderPass;
    class RenderResources;
    class RenderSettingsTool;
    class Scene;
    class SwapChain;
    class RenderViewportTool;
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
        void ProcessFrame();
        void EndFrame();

        void OnUpdate();
        void OnRender();

        void RequestShutdown();

    protected:
        std::unique_ptr<benzin::Window> m_MainWindow;
        std::unique_ptr<benzin::Backend> m_Backend;
        std::unique_ptr<benzin::Device> m_Device;
        std::unique_ptr<benzin::SwapChain> m_SwapChain;

        benzin::TickTimer m_FrameTimer;
        benzin::TickTimer m_AnimationTimer;
        benzin::IntervalTimer m_1SecIntervalTimer;

        FpsCounter m_FpsCounter;

        std::unique_ptr<benzin::Scene> m_Scene;
        std::unique_ptr<benzin::FlyCameraController> m_FlyCameraController;

        std::unique_ptr<benzin::RenderResources> m_RenderResources;
        std::unique_ptr<benzin::RenderSettings> m_RenderSettings;
        std::vector<std::unique_ptr<benzin::RenderPass>> m_RenderPasses;
        benzin::ImGuiPass* m_ImGuiPass = nullptr;

        std::unique_ptr<benzin::ImGuiManager> m_ImGuiManager;
        benzin::RenderViewportTool* m_RenderViewportTool = nullptr;
        benzin::RenderSettingsTool* m_RenderSettingsTool = nullptr;

        bool m_IsRunning = true;
        bool m_IsVerticalSyncEnabled = true;

    private:
        BottomPanelTool* m_BottomPanelTool = nullptr;
        BottomPanelTool::RunnerTimings m_Timings{};

        uint32_t m_PendingWidth = 0;
        uint32_t m_PendingHeight = 0;
    };

}
