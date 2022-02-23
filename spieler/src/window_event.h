#pragma once

#include "event.h"

namespace Spieler
{

    class WindowCloseEvent : public Event
    {
    public:
        EVENT_CLASS_TYPE(WindowCloseEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowResizingEvent : public Event
    {
    public:
        EVENT_CLASS_TYPE(WindowResizingEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        WindowResizingEvent(std::uint32_t width, std::uint32_t height)
            : m_Width(width)
            , m_Height(height)
        {}

    public:

        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }

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
    public:
        EVENT_CLASS_TYPE(WindowEnterResizingEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowExitResizingEvent : public Event
    {
    public:
        EVENT_CLASS_TYPE(WindowExitResizingEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        WindowExitResizingEvent(std::uint32_t width, std::uint32_t height)
            : m_Width(width)
            , m_Height(height)
        {}

    public:
        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }

        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(m_Width) + ", " + std::to_string(m_Height);
        }

    private:
        std::uint32_t m_Width;
        std::uint32_t m_Height;
    };

    class WindowMaximizedEvent : public Event
    {
    public:
        EVENT_CLASS_TYPE(WindowMaximizedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowMinimizedEvent : public Event
    {
    public:
        EVENT_CLASS_TYPE(WindowMinimizedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowRestoredEvent : public Event
    {
    public:
        EVENT_CLASS_TYPE(WindowRestoredEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowFocusedEvent : public Event
    {
    public:
        EVENT_CLASS_TYPE(WindowFocusedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

    class WindowUnfocusedEvent : public Event
    {
    public:
        EVENT_CLASS_TYPE(WindowUnfocusedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        std::string ToString() const override { return GetName(); }
    };

} // namespace Spieler