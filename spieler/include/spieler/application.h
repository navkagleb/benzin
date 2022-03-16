#pragma once

#include "common.h"
#include "timer.h"
#include "layer_stack.h"
#include "window/window.h"
#include "window/window_event.h"
#include "window/key_event.h"
#include "renderer/renderer.h"
#include "renderer/descriptor_heap.h"

namespace Spieler
{

    class Application
    {
    public:
        static Application& GetInstance();

    protected:
        Application();

    public:
        Renderer& GetRenderer() { return m_Renderer; }
        const Renderer& GetRenderer() const { return m_Renderer; }

    public:
        bool InitInternal(const std::string& title, std::uint32_t width, std::uint32_t height);

        virtual bool InitExternal() = 0;
        
        int Run();

        template <typename T, typename... Args>
        std::shared_ptr<T> PushLayer(Args&&... args) { return m_LayerStack.PushLayer<T>(std::forward<Args>(args)...); }

    private:
        bool InitWindow(const std::string& title, std::uint32_t width, std::uint32_t height);

#if SPIELER_USE_IMGUI
        bool InitImGui();
#endif

        void OnUpdate(float dt);
        bool OnRender(float dt);

#if SPIELER_USE_IMGUI
        void OnImGuiRender(float dt);
#endif

        void OnClose();

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