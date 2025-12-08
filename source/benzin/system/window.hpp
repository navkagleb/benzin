#pragma once

namespace benzin
{

    class Event;

    struct WindowCreation
    {
        std::string_view m_Title;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

    class Window
    {
    public:
        friend struct WindowRegisterManager;

        using PreMessageHandlerCallback = std::function<LRESULT(HWND windowHandle, UINT messageCode, WPARAM wparam, LPARAM lparam)>;
        using EventCallback = std::function<void(Event& event)>;

        Window(const WindowCreation& creation);
        ~Window();

        auto GetWin64Window() const { return m_Win64Window; }

        auto GetWidth() const { return m_Width; }
        auto GetHeight() const { return m_Height; }

        auto IsResizing() const { return m_IsResizing; }
        auto IsMinimized() const { return m_IsMinimized; }
        auto IsMaximized() const { return m_IsMaximized; }
        auto IsFocused() const { return m_IsFocused; }

        void SetPreMessageHandlerCallback(PreMessageHandlerCallback&& callback) { m_PreMessageHandlerCallback = std::move(callback); }
        void SetEventCallback(EventCallback&& callback) { m_EventCallback = std::move(callback); }

        void ProcessEvents();

        void SetTitle(std::string_view title);
        void SetVisible(bool isVisible);

    private:
        static LRESULT MessageHandler(HWND windowHandle, UINT messageCode, WPARAM wparam, LPARAM lparam);
        static bool HandleWindowEvents(Window& window, UINT messageCode, WPARAM wparam, LPARAM lparam);
        static bool HandleMouseEvents(Window& window, UINT messageCode, WPARAM wparam, LPARAM lparam);
        static bool HandleKeyEvents(Window& window, UINT messageCode, WPARAM wparam, LPARAM lparam);

        template <typename Event, typename... Args>
        void CreateAndPushEvent(Args&&... args)
        {
            Event event{ std::forward<Args>(args)... };
            m_EventCallback(event);
        }

        HWND m_Win64Window = nullptr;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        bool m_IsResizing = false;
        bool m_IsMinimized = false;
        bool m_IsMaximized = false;
        bool m_IsFocused = true;

        PreMessageHandlerCallback m_PreMessageHandlerCallback;
        EventCallback m_EventCallback;
    };

}
