#pragma once

#include "benzin/system/event.hpp"
#include "benzin/system/input.hpp"

namespace benzin
{

    class Window
    {
    private:
        friend LRESULT MessageHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    public:
        using EventCallbackFunction = std::function<void(Event& event)>;

    public:
        Window(std::string_view title, uint32_t width, uint32_t height, const EventCallbackFunction& eventCallback);
        ~Window();

    public:
        const HWND GetWin64Window() const { return m_Win64Window; }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        bool IsResizing() const { return m_IsResizing; }
        bool IsMinimized() const { return m_IsMinimized; }
        bool IsMaximized() const { return m_IsMaximized; }

    public:
        void ProcessEvents();

        void SetTitle(std::string_view title);
        void SetVisible(bool isVisible);

        void SetEventCallbackFunction(const EventCallbackFunction& callback);

    private:
        HWND m_Win64Window{ nullptr };

        uint32_t m_Width{ 0 };
        uint32_t m_Height{ 0 };
        bool m_IsResizing{ false };
        bool m_IsMinimized{ false };
        bool m_IsMaximized{ false };

        EventCallbackFunction m_EventCallback;
    };

} // namespace benzin
