#pragma once

#include "spieler/core/assert.hpp"

#include "spieler/core/timer.hpp"
#include "spieler/core/layer_stack.hpp"

#include "spieler/system/window.hpp"
#include "spieler/system/window_event.hpp"
#include "spieler/system/key_event.hpp"

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
        const Window& GetWindow() const { return *m_Window; }

    public:
        bool InitInternal(const std::string& title, uint32_t width, uint32_t height);

        virtual bool InitExternal() = 0;
        
        void Run();

        template <typename T, typename... Args>
        std::shared_ptr<T> PushLayer(Args&&... args) { return m_LayerStack.PushLayer<T>(std::forward<Args>(args)...); }

    private:
        bool InitWindow(const std::string& title, uint32_t width, uint32_t height);
        bool InitImGui();

        void OnUpdate(float dt);
        bool OnRender(float dt);
        void OnImGuiRender(float dt);

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

        void CalcStats();

        void UpdateScreenViewport();
        void UpdateScreenScissorRect();

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

    private:
        ApplicationProps m_ApplicationProps;
        Timer m_Timer;
        LayerStack m_LayerStack;

        renderer::Viewport m_ScreenViewport;
        renderer::ScissorRect m_ScreenScissorRect;
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