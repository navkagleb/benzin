#pragma once

#include "event.h"

namespace Spieler
{

    class WindowResizeEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowResizeEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        WindowResizeEvent(int width, int height);
        ~WindowResizeEvent() override = default;

        [[nodiscard]] inline int GetWidth() const { return m_Width; }
        [[nodiscard]] inline int GetHeight() const { return m_Height; }
        [[nodiscard]] std::string ToString() const override;

    private:
        int m_Width;
        int m_Height;
    };

    class WindowCloseEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowCloseEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        inline std::string ToString() const override { return GetName(); }
    };

    class WindowMaximizedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowMaximizedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        inline std::string ToString() const override { return GetName(); }
    };

    class WindowMinimizedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowMinimizedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        inline std::string ToString() const override { return GetName(); }
    };

    class WindowFocusedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowFocusedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        WindowFocusedEvent() = default;
        ~WindowFocusedEvent() override = default;

        inline std::string ToString() const override { return GetName(); }
    };

    class WindowUnfocusedEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowUnfocusedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        inline std::string ToString() const override { return GetName(); }
    };

    class WindowCursorEnteredEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowCursorEnteredEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        inline std::string ToString() const override { return GetName(); }
    };

    class WindowCursorLeftEvent : public Event
    {
        EVENT_CLASS_TYPE(WindowCursorLeftEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Window)

    public:
        inline std::string ToString() const override { return GetName(); }
    };

} // namespace Spieler