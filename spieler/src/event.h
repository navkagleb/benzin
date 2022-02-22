#pragma once

#include <string>

namespace Spieler
{

#define EVENT_CLASS_TYPE(ClassName)                                                                     \
    static EventType GetStaticEventType() { return EventType_##ClassName; }                             \
    inline EventType GetEventType() const override { return GetStaticEventType(); }                     \
    inline std::string GetName() const override { return #ClassName; }

#define EVENT_CLASS_CATEGORY(category)                                                                  \
    inline int GetCategoryFlags() const override { return category; }

    enum EventType : std::uint32_t
    {
        EventType_None = 0,

        EventType_WindowResizeEvent,
        EventType_WindowCloseEvent,
        EventType_WindowMaximizedEvent,
        EventType_WindowMinimizedEvent,
        EventType_WindowFocusedEvent,
        EventType_WindowUnfocusedEvent,
        EventType_WindowCursorEnteredEvent,
        EventType_WindowCursorLeftEvent,

        EventType_MouseMovedEvent,
        EventType_MouseScrolledEvent,
        EventType_MouseButtonPressedEvent,
        EventType_MouseButtonReleasedEvent,

        EventType_KeyPressedEvent,
        EventType_KeyReleasedEvent,
        EventType_KeyTypedEvent,
    };

    enum EventCategory : std::uint32_t
    {
        EventCategory_None          = 0,

        EventCategory_Window        = 1 << 0,
        EventCategory_Input         = 1 << 1,
        EventCategory_Mouse         = 1 << 2,
        EventCategory_Keyboard      = 1 << 3,
        EventCategory_MouseButton   = 1 << 4
    };

    class Event
    {
    public:
        virtual ~Event() = default;

        virtual EventType GetEventType() const = 0;
        virtual std::string GetName() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual std::string ToString() const = 0;

        inline bool IsInCategory(EventCategory category) const { return (GetCategoryFlags() & category) != EventCategory_None; }

        friend class EventDispatcher;

    protected:
        Event()
            : m_Handled(false) {}

    protected:
        bool m_Handled;
    };

} // namespace Spieler