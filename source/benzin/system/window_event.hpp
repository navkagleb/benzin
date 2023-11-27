#pragma once

#include "benzin/system/event.hpp"

namespace benzin
{

    class WindowSizeEvent
    {
    public:
        WindowSizeEvent(uint32_t width, uint32_t height)
            : m_Width{ width }
            , m_Height{ height }
        {}

    public:
        template <typename T = uint32_t> T GetWidth() const { return static_cast<T>(m_Width); }
        template <typename T = uint32_t> T GetHeight() const { return static_cast<T>(m_Height); }

        float GetAspectRatio() const { return GetWidth<float>() / GetHeight<float>(); }

    private:
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

    class WindowCloseEvent : public EventInfo<EventType::WindowCloseEvent, EventCategoryFlag::Window>
    {};

    class WindowResizingEvent : public EventInfo<EventType::WindowResizingEvent, EventCategoryFlag::Window>, public WindowSizeEvent
    {
    public:
        WindowResizingEvent(uint32_t width, uint32_t height)
            : WindowSizeEvent{ width, height }
        {}
    };

    class WindowEnterResizingEvent : public EventInfo<EventType::WindowEnterResizingEvent, EventCategoryFlag::Window>
    {};

    class WindowExitResizingEvent : public EventInfo<EventType::WindowExitResizingEvent, EventCategoryFlag::Window>
    {};

    class WindowMaximizedEvent : public EventInfo<EventType::WindowMaximizedEvent, EventCategoryFlag::Window>
    {};

    class WindowMinimizedEvent : public EventInfo<EventType::WindowMinimizedEvent, EventCategoryFlag::Window>
    {};

    class WindowRestoredEvent : public EventInfo<EventType::WindowRestoredEvent, EventCategoryFlag::Window>
    {};

    class WindowFocusedEvent : public EventInfo<EventType::WindowFocusedEvent, EventCategoryFlag::Window>
    {};

    class WindowUnfocusedEvent : public EventInfo<EventType::WindowUnfocusedEvent, EventCategoryFlag::Window>
    {};

    class WindowResizedEvent : public EventInfo<EventType::WindowResizedEvent, EventCategoryFlag::Window>, public WindowSizeEvent
    {
    public:
        WindowResizedEvent(uint32_t width, uint32_t height)
            : WindowSizeEvent{ width, height }
        {}
    };

} // namespace benzin
