#pragma once

#include "benzin/system/event.hpp"
#include "benzin/system/key_code.hpp"

namespace benzin
{

    class KeyCodeEvent
    {
    protected:
        explicit KeyCodeEvent(KeyCode keyCode)
            : m_KeyCode{ keyCode }
        {}

    public:
        KeyCode GetKeyCode() const { return m_KeyCode; }

    protected:
        KeyCode m_KeyCode = KeyCode::Unknown;
    };

    class KeyPressedEvent : public EventInfo<EventType::KeyPressedEvent, EventCategoryFlag::Input, EventCategoryFlag::Keyboard>, public KeyCodeEvent
    {
    public:
        KeyPressedEvent(KeyCode keyCode, bool isRepeated)
            : KeyCodeEvent{ keyCode }
            , m_IsRepeated{ isRepeated }
        {}
       
    public:
        bool IsRepeated() const { return m_IsRepeated; }

    private:
        bool m_IsRepeated = false;
    };

    class KeyReleasedEvent : public EventInfo<EventType::KeyReleasedEvent, EventCategoryFlag::Input, EventCategoryFlag::Keyboard>, public KeyCodeEvent
    {
    public:
        explicit KeyReleasedEvent(KeyCode keyCode)
            : KeyCodeEvent{ keyCode }
        {}
    };

    class KeyTypedEvent : public EventInfo<EventType::KeyTypedEvent, EventCategoryFlag::Input, EventCategoryFlag::Keyboard>
    {
    public:
        explicit KeyTypedEvent(char character)
            : m_Character{ character }
        {}

        char GetCharacter() const { return m_Character; }

    private:
        char m_Character = '\0';
    };

} // namespace benzin
