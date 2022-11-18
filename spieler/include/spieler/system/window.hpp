#pragma once

#include "spieler/system/event.hpp"
#include "spieler/system/input.hpp"

namespace spieler
{

    class Window
    {
    private:
        friend LRESULT MessageHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    public:
        using EventCallbackFunction = std::function<void(Event& event)>;

    public:
        Window(const std::string& title, uint32_t width, uint32_t height);
        ~Window();

    public:
        const HWND GetWin64Window() const { return m_Win64Window; }

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        bool IsResizing() const { return m_IsResizing; }
        bool IsMinimized() const { return m_IsMinimized; }
        bool IsMaximized() const { return m_IsMaximized; }

        float GetAspectRatio() const { return static_cast<float>(m_Width) / m_Height; }

    public:
        void ProcessEvents();

        void SetTitle(const std::string_view& title);
        void SetVisible(bool isVisible);

        void SetEventCallbackFunction(const EventCallbackFunction& callback);

    protected:
        HWND m_Win64Window{ nullptr };

        uint32_t m_Width{ 0 };
        uint32_t m_Height{ 0 };
        bool m_IsResizing{ false };
        bool m_IsMinimized{ false };
        bool m_IsMaximized{ false };

        EventCallbackFunction m_EventCallback;
    };

} // namespace spieler
