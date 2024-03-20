#pragma once

#include "benzin/system/event.hpp"
#include "benzin/system/key_code.hpp"

namespace benzin
{

    template <EventType T>
    class KeyEventInfo : public EventInfo<T>
    {
    public:
        KeyEventInfo()
            : EventInfo<T>{ EventCategoryFlag::Input | EventCategoryFlag::Keyboard }
        {}
    };

    class KeyCodeEvent
    {
    protected:
        explicit KeyCodeEvent(KeyCode keyCode)
            : m_KeyCode{ keyCode }
        {}

    public:
        auto GetKeyCode() const { return m_KeyCode; }

    protected:
        KeyCode m_KeyCode = KeyCode::Unknown;
    };

    class KeyPressedEvent
        : public KeyEventInfo<EventType::KeyPressedEvent>
        , public KeyCodeEvent
    {
    public:
        KeyPressedEvent(KeyCode keyCode, bool isRepeated)
            : KeyCodeEvent{ keyCode }
            , m_IsRepeated{ isRepeated }
        {}
       
    public:
        auto IsRepeated() const { return m_IsRepeated; }

    private:
        bool m_IsRepeated = false;
    };

    class KeyReleasedEvent
        : public KeyEventInfo<EventType::KeyReleasedEvent>
        , public KeyCodeEvent
    {
    public:
        explicit KeyReleasedEvent(KeyCode keyCode)
            : KeyCodeEvent{ keyCode }
        {}
    };

    class KeyTypedEvent : public KeyEventInfo<EventType::KeyTypedEvent>
    {
    public:
        explicit KeyTypedEvent(char character)
            : m_Character{ character }
        {}

        auto GetCharacter() const { return m_Character; }

    private:
        char m_Character = '\0';
    };

} // namespace benzin
