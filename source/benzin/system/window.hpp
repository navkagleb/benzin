#pragma once

#include "benzin/system/event.hpp"
#include "benzin/system/input.hpp"

namespace benzin
{

    using WindowEventCallback = std::function<void(Event& event)>;

    struct WindowCreation
    {
        std::string_view Title;
        uint32_t Width = 0;
        uint32_t Height = 0;
        WindowEventCallback EventCallback;
    };

    class Window
    {
    private:
        friend WindowsMessageHandlerDeclaration();

    public:
        Window(const WindowCreation& creation);
        ~Window();

    public:
        const HWND GetWin64Window() const { return m_Win64Window; }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        bool IsResizing() const { return m_IsResizing; }
        bool IsMinimized() const { return m_IsMinimized; }
        bool IsMaximized() const { return m_IsMaximized; }
        bool IsFocused() const { return m_IsFocused; }

    public:
        void ProcessEvents();

        void SetTitle(std::string_view title);
        void SetVisible(bool isVisible);

    private:
        template <typename Event, typename... Args>
        void CreateAndPushEvent(Args&&... args);

    private:
        HWND m_Win64Window = nullptr;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        bool m_IsResizing = false;
        bool m_IsMinimized = false;
        bool m_IsMaximized = false;
        bool m_IsFocused = true;

        WindowEventCallback m_EventCallback;
    };

    template <typename Event, typename... Args>
    void Window::CreateAndPushEvent(Args&&... args)
    {
        if (m_EventCallback)
        {
            Event event{ std::forward<Args>(args)... };
            m_EventCallback(event);
        }
    }

} // namespace benzin
