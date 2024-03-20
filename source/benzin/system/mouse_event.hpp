#pragma once

#include "benzin/system/event.hpp"
#include "benzin/system/mouse_button.hpp"

namespace benzin
{

    template <EventType T>
    class MouseEventInfo : public EventInfo<T>
    {
    public:
        MouseEventInfo()
            : EventInfo<T>{ EventCategoryFlag::Input | EventCategoryFlag::Mouse }
        {}
    };

    template <EventType T>
    class MouseButtonEventInfo : public EventInfo<T>
    {
    public:
        MouseButtonEventInfo()
            : EventInfo<T>{ EventCategoryFlag::Input | EventCategoryFlag::Mouse | EventCategoryFlag::MouseButton }
        {}
    };

    class MousePositionEvent
    {
    public:
        MousePositionEvent(int32_t x, int32_t y)
            : m_X{ x }
            , m_Y{ y }
        {}

    public:
        template <typename T = int32_t> T GetX() const { return (T)m_X; }
        template <typename T = int32_t> T GetY() const { return (T)m_Y; }

    protected:
        int32_t m_X = 0;
        int32_t m_Y = 0;
    };

    class MouseButtonEvent : public MousePositionEvent
    {
    public:
        MouseButtonEvent(MouseButton button, int32_t x, int32_t y)
            : MousePositionEvent{ x, y }
            , m_Button{ button }
        {}

    public:
        auto GetButton() const { return m_Button; }

    protected:
        MouseButton m_Button = MouseButton::Unknown;
    };

    class MouseMovedEvent
        : public MouseEventInfo<EventType::MouseMovedEvent>
        , public MousePositionEvent
    {
    public:
        MouseMovedEvent(int32_t x, int32_t y)
            : MousePositionEvent{ x, y }
        {}
    };

    class MouseScrolledEvent : public MouseEventInfo<EventType::MouseScrolledEvent>
    {
    public:
        MouseScrolledEvent(int8_t offsetX)
            : m_OffsetX{ offsetX }
        {}
        
    public:
        auto GetOffsetX() const { return m_OffsetX; }

    private:
        int8_t m_OffsetX = 0;
    };

    class MouseButtonPressedEvent
        : public MouseButtonEventInfo<EventType::MouseButtonPressedEvent>
        , public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(MouseButton button, int32_t x, int32_t y)
            : MouseButtonEvent{ button, x, y }
        {}
    };

    class MouseButtonReleasedEvent
        : public MouseButtonEventInfo<EventType::MouseButtonReleasedEvent>
        , public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(MouseButton button, int32_t x, int32_t y)
            : MouseButtonEvent{ button, x, y }
        {}
    };

} // namespace benzin
