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
    private:
        SPIELER_NON_COPYABLE(Application)
        SPIELER_NON_MOVEABLE(Application)

    public:
        template <typename ClientApplication>
        static Application* CreateInstance(const std::string& title, uint32_t width, uint32_t height);
        static Application* GetInstance();

    protected:
        Application();

    public:
        virtual ~Application();

    public:
        Window& GetWindow() { return *m_Window; }
        const Window& GetWindow() const { return *m_Window; }

    public:
        bool InitInternal(const std::string& title, uint32_t width, uint32_t height);

        virtual bool InitExternal() = 0;
        
        void Run();

    private:
        bool InitWindow(const std::string& title, uint32_t width, uint32_t height);

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

    private:
        struct ApplicationProps
        {
            bool IsRunning{ false };
            bool IsPaused{ false };
            bool IsFullscreen{ false };
        };

    private:
        inline static Application* g_Instance{ nullptr };

    protected:
        std::unique_ptr<Window> m_Window;
        LayerStack m_LayerStack;

    private:
        ApplicationProps m_ApplicationProps;
        Timer m_Timer;

        //std::shared_ptr<ImGuiLayer> m_ImGuiLayer;
    };

    template <typename ClientApplication>
    Application* Application::CreateInstance(const std::string& title, uint32_t width, uint32_t height)
    {
        SPIELER_ASSERT(!g_Instance);

        g_Instance = new ClientApplication;

        if (!g_Instance->InitInternal(title, width, height))
        {
            delete g_Instance;
            return nullptr;
        }

        return g_Instance;
    }

    // To be defined in client!
    Application* CreateApplication();

} // namespace spieler