#pragma once

#include "spieler/system/event.hpp"
#include "spieler/system/mouse_button.hpp"

namespace spieler
{
    class MousePosition
    {
    public:
        MousePosition(uint32_t x, uint32_t y)
            : m_X(x)
            , m_Y(y)
        {}

    public:
        template <typename T = uint32_t> T GetX() const { return static_cast<T>(m_X); }
        template <typename T = uint32_t> T GetY() const { return static_cast<T>(m_Y); }

    protected:
        uint32_t m_X;
        uint32_t m_Y;
    };

    class MouseMovedEvent : public Event, public MousePosition
    {
        EVENT_CLASS_TYPE(MouseMovedEvent)
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Mouse)

    public:
        MouseMovedEvent(int32_t x, int32_t y)
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
        uint8_t GetOffsetX() const { return m_OffsetX; }

        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(m_OffsetX);
        }

    private:
        uint8_t m_OffsetX;
    };

    class MouseButtonEvent : public Event, public MousePosition
    {
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Mouse | EventCategory_MouseButton)

    public:
        MouseButtonEvent(MouseButton button, int32_t x, int32_t y)
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
        MouseButtonPressedEvent(MouseButton button, int32_t x, int32_t y)
            : MouseButtonEvent(button, x, y)
        {}

    public:
        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(static_cast<int32_t>(m_Button)) + "(" + std::to_string(m_X) + ", " + std::to_string(m_Y) + ")";
        }
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
        EVENT_CLASS_TYPE(MouseButtonReleasedEvent);

    public:
        MouseButtonReleasedEvent(MouseButton button, int32_t x, int32_t y)
            : MouseButtonEvent(button, x, y)
        {}

    public:
        std::string ToString() const override
        {
            return GetName() + ": " + std::to_string(static_cast<uint32_t>(m_Button)) + "(" + std::to_string(m_X) + ", " + std::to_string(m_Y) + ")";
        }
    };

} // namespace spieler