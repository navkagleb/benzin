#pragma once

#include "benzin/system/event.hpp"
#include "benzin/system/key_code.hpp"

namespace benzin
{

    class KeyEvent : public Event
    {
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Keyboard)

    protected:
        explicit KeyEvent(KeyCode keyCode)
            : m_KeyCode(keyCode)
        {}

    public:
        KeyCode GetKeyCode() const { return m_KeyCode; }

    protected:
        KeyCode m_KeyCode;
    };

    class KeyPressedEvent : public KeyEvent
    {
        EVENT_CLASS_TYPE(KeyPressedEvent)

    public:
        KeyPressedEvent(KeyCode keyCode, bool isRepeated)
            : KeyEvent(keyCode)
            , m_IsRepeated(isRepeated)
        {}
       
    public:
        bool IsRepeated() const { return m_IsRepeated; }

        std::string ToString() const override
        {
            return GetName() + ": " + static_cast<char>(m_KeyCode) + "(" + std::to_string(static_cast<uint32_t>(m_KeyCode)) + ", " + std::to_string(m_IsRepeated) + ")";
        }

    private:
        bool m_IsRepeated;
    };

    class KeyReleasedEvent : public KeyEvent
    {
        EVENT_CLASS_TYPE(KeyReleasedEvent)

    public:
        explicit KeyReleasedEvent(KeyCode keyCode)
            : KeyEvent(keyCode)
        {}
        
    public:
        std::string ToString() const override
        {
            return GetName() + ": " + static_cast<char>(m_KeyCode) + "(" + std::to_string(static_cast<uint32_t>(m_KeyCode)) + ")";
        }
    };

    class KeyTypedEvent : public Event
    {
        EVENT_CLASS_TYPE(KeyTypedEvent);
        EVENT_CLASS_CATEGORY(EventCategory_Input | EventCategory_Keyboard)

    public:
        explicit KeyTypedEvent(char character)
            : m_Character(character)
        {}
       
        std::string ToString() const override
        {
            return GetName() + ": " + m_Character;
        }

    private:
        char m_Character;
    };
} // namespace benzin