#pragma once

#include "event.hpp"

namespace Spieler
{

    class WindowCloseEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowCloseEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowResizingEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowResizingEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        WindowResizingEvent(std::uint32_t width, std::uint32_t height)
            : m_Width(width)
            , m_Height(height)
        {}

    public:
        std::uint32_t GetWidth() const { return m_Width; }
        std::uint32_t GetHeight() const { return m_Height; }

        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(m_Width) + ", " + std::to_string(m_Height);
        }

    private:
        std::uint32_t m_Width;
        std::uint32_t m_Height;
    };

    class WindowEnterResizingEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowEnterResizingEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowExitResizingEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowExitResizingEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowMaximizedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowMaximizedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowMinimizedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowMinimizedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowRestoredEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowRestoredEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowFocusedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowFocusedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowUnfocusedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowUnfocusedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowResizedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowResizedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        WindowResizedEvent(std::uint32_t width, std::uint32_t height)
            : m_Width(width)
            , m_Height(height)
        {}

    public:
        template <typename T = std::uint32_t> T GetWidth() const { return static_cast<T>(m_Width); }
        template <typename T = std::uint32_t> T GetHeight() const { return static_cast<T>(m_Height); }

        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(m_Width) + ", " + std::to_string(m_Height) + "!!!";
        }

    private:
        std::uint32_t m_Width;
        std::uint32_t m_Height;
    };

} // namespace Spieler