#pragma once

#include "event.hpp"
#include "mouse_button.hpp"

namespace Spieler
{
    class MousePosition
    {
    public:
        MousePosition(std::int32_t x, std::int32_t y)
            : m_X(x)
            , m_Y(y)
        {}

    public:
        std::int32_t GetX() const { return m_X; }
        std::int32_t GetY() const { return m_Y; }

    protected:
        std::int32_t m_X;
        std::int32_t m_Y;
    };

    class MouseMovedEvent : public Event, public MousePosition
    {
        EVENT_CLASS_TYPE(MouseMovedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Mouse)

    public:
        MouseMovedEvent(std::int32_t x, std::int32_t y)
            : MousePosition(x, y)
        {}

    public:
        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(m_X) + ", " + std::to_string(m_Y);
        }
    };

    class MouseScrolledEvent : public Event
    {
        EVENT_CLASS_TYPE(MouseScrolledEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Mouse)

    public:
        MouseScrolledEvent(int offsetX)
            : m_OffsetX(offsetX)
        {}
        
    public:
        int GetOffsetX() const { return m_OffsetX; }

        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(m_OffsetX);
        }

    private:
        int m_OffsetX;
    };

    class MouseButtonEvent : public Event, public MousePosition
    {
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Mouse | EventCategory_MouseButton)

    public:
        MouseButtonEvent(MouseButton button, std::int32_t x, std::int32_t y)
            : MousePosition(x, y)
            , m_Button(button)
        {}
        
    public:
        MouseButton GetButton() const { return m_Button; }

    protected:
        MouseButton m_Button;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
        EVENT_CLASS_TYPE(MouseButtonPressedEvent)

    public:
        MouseButtonPressedEvent(MouseButton button, std::int32_t x, std::int32_t y)
            : MouseButtonEvent(button, x, y)
        {}

    public:
        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(m_Button) + "(" + std::to_string(m_X) + ", " + std::to_string(m_Y) + ")";
        }
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
        EVENT_CLASS_TYPE(MouseButtonReleasedEvent);

    public:
        MouseButtonReleasedEvent(MouseButton button, std::int32_t x, std::int32_t y)
            : MouseButtonEvent(button, x, y)
        {}

    public:
        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(m_Button) + "(" + std::to_string(m_X) + ", " + std::to_string(m_Y) + ")";
        }
    };

} // namespace Spieler