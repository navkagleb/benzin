#pragma once

#include "spieler/core/assert.hpp"

#include "spieler/core/timer.hpp"
#include "spieler/core/layer_stack.hpp"
#include "spieler/core/imgui_layer.hpp"

#include "spieler/system/window.hpp"
#include "spieler/system/window_event.hpp"

#include "spieler/renderer/renderer.hpp"

namespace spieler
{

    class Application
    {
    public:
        struct Config
        {
            std::string Title;
            uint32_t Width{ 0 };
            uint32_t Height{ 0 };
        };

    private:
        SPIELER_NON_COPYABLE(Application)
        SPIELER_NON_MOVEABLE(Application)

    public:
        static void CreateInstance();
        static void DestroyInstance();
        static Application& GetInstance();

    protected:
        Application(const Config& config);

    public:
        virtual ~Application();

    public:
        Window& GetWindow() { return *m_Window; }
        const Window& GetWindow() const { return *m_Window; }

    public:
        void Run();

    private:
        void WindowEventCallback(Event& event);

        bool OnWindowClose(WindowCloseEvent& event);
        bool OnWindowMinimized(WindowMinimizedEvent& event);
        bool OnWindowMaximized(WindowMaximizedEvent& event);
        bool OnWindowRestored(WindowRestoredEvent& event);
        bool OnWindowEnterResizing(WindowEnterResizingEvent& event);
        bool OnWindowExitResizing(WindowExitResizingEvent& event);
        bool OnWindowResized(WindowResizedEvent& event);
        bool OnWindowFocused(WindowFocusedEvent& event);
        bool OnWindowUnfocused(WindowUnfocusedEvent& event);

        void CalcStats(float dt);

    protected:
        std::unique_ptr<Window> m_Window;
        LayerStack m_LayerStack;

    private:
        bool IsRunning{ false };
        bool IsPaused{ false };
        Timer m_Timer;

        std::shared_ptr<ImGuiLayer> m_ImGuiLayer;
    };

    // To be defined in client!
    Application* CreateApplication();

} // namespace spieler
