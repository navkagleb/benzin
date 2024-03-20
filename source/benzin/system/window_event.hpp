#pragma once

#include "benzin/system/event.hpp"

namespace benzin
{

    template <EventType T>
    class WindowEventInfo : public EventInfo<T>
    {
    public:
        WindowEventInfo()
            : EventInfo<T>{ EventCategoryFlag::Window }
        {}
    };

    class WindowSizeEvent
    {
    public:
        WindowSizeEvent(uint32_t width, uint32_t height)
            : m_Width{ width }
            , m_Height{ height }
        {}

    public:
        template <typename T = uint32_t> T GetWidth() const { return (T)m_Width; }
        template <typename T = uint32_t> T GetHeight() const { return (T)m_Height; }

        float GetAspectRatio() const { return GetWidth<float>() / GetHeight<float>(); }

    private:
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

    class WindowCloseEvent : public WindowEventInfo<EventType::WindowCloseEvent> {};

    class WindowMaximizedEvent : public WindowEventInfo<EventType::WindowMaximizedEvent> {};

    class WindowMinimizedEvent : public WindowEventInfo<EventType::WindowMinimizedEvent> {};

    class WindowRestoredEvent : public WindowEventInfo<EventType::WindowRestoredEvent> {};

    class WindowFocusedEvent : public WindowEventInfo<EventType::WindowFocusedEvent> {};

    class WindowUnfocusedEvent : public WindowEventInfo<EventType::WindowUnfocusedEvent> {};

    class WindowEnterResizingEvent : public WindowEventInfo<EventType::WindowEnterResizingEvent> {};

    class WindowExitResizingEvent : public WindowEventInfo<EventType::WindowExitResizingEvent> {};

    class WindowResizingEvent
        : public WindowEventInfo<EventType::WindowResizingEvent>
        , public WindowSizeEvent
    {
    public:
        WindowResizingEvent(uint32_t width, uint32_t height)
            : WindowSizeEvent{ width, height }
        {}
    };

    class WindowResizedEvent
        : public WindowEventInfo<EventType::WindowResizedEvent>
        , public WindowSizeEvent
    {
    public:
        WindowResizedEvent(uint32_t width, uint32_t height)
            : WindowSizeEvent{ width, height }
        {}
    };

} // namespace benzin
