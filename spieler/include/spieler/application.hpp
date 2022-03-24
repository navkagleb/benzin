#pragma once

#include "common.hpp"
#include "timer.hpp"
#include "layer_stack.hpp"
#include "window/window.hpp"
#include "window/window_event.hpp"
#include "window/key_event.hpp"
#include "renderer/renderer.hpp"
#include "renderer/descriptor_heap.hpp"

namespace Spieler
{

    class Application
    {
    public:
        static Application& GetInstance();

    protected:
        Application();

    public:
        ~Application();

    public:
        Renderer& GetRenderer() { return m_Renderer; }
        const Renderer& GetRenderer() const { return m_Renderer; }

        const Timer& GetTimer() const { return m_Timer; }

    public:
        bool InitInternal(const std::string& title, std::uint32_t width, std::uint32_t height);

        virtual bool InitExternal() = 0;
        
        int Run();

        template <typename T, typename... Args>
        std::shared_ptr<T> PushLayer(Args&&... args) { return m_LayerStack.PushLayer<T>(std::forward<Args>(args)...); }

    private:
        bool InitWindow(const std::string& title, std::uint32_t width, std::uint32_t height);
        bool InitRenderer();
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
        inline static Application*  g_Instance{ nullptr };

    protected:
        Window                      m_Window;
        Renderer                    m_Renderer;

    private:
        ApplicationProps            m_ApplicationProps;
        Timer                       m_Timer;
        LayerStack                  m_LayerStack;

        DescriptorHeap              m_ImGuiDescriptorHeap;

        Viewport                    m_ScreenViewport;
        ScissorRect                 m_ScreenScissorRect;
    };

    // To be defined in client!
    Application* CreateApplication();

} // namespace Spieler