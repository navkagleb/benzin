#pragma once

namespace spieler
{

#define EVENT_CLASS_TYPE(ClassName)                                                                     \
public:                                                                                                 \
    static EventType GetStaticEventType() { return EventType::ClassName; }                              \
                                                                                                        \
public:                                                                                                 \
    EventType GetEventType() const override { return GetStaticEventType(); }                            \
    std::string GetName() const override { return #ClassName; }                                         \

#define EVENT_CLASS_CATEGORY(category)                                                                  \
    int32_t GetCategoryFlags() const override { return category; }

    enum class EventType : uint8_t
    {
        None = 0,

        // Window events
        WindowCloseEvent,
        WindowResizingEvent,
        WindowEnterResizingEvent,
        WindowExitResizingEvent,
        WindowMaximizedEvent,
        WindowMinimizedEvent,
        WindowRestoredEvent,
        WindowFocusedEvent,
        WindowUnfocusedEvent,
        WindowResizedEvent,

        // Mouse events
        MouseMovedEvent,
        MouseScrolledEvent,
        MouseButtonPressedEvent,
        MouseButtonReleasedEvent,

        // Key events
        KeyPressedEvent,
        KeyReleasedEvent,
        KeyTypedEvent,
    };

    enum EventCategory : int32_t
    {
        EventCategory_None = 0,
        EventCategory_Window = 1 << 0,
        EventCategory_Input = 1 << 1,
        EventCategory_Mouse = 1 << 2,
        EventCategory_Keyboard = 1 << 3,
        EventCategory_MouseButton = 1 << 4
    };

    class Event
    {
    public:
        friend class EventDispatcher;

    public:
        virtual ~Event() = default;

    public:
        bool IsHandled() const { return m_IsHandled; }

    public:
        virtual EventType GetEventType() const = 0;
        virtual std::string GetName() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual std::string ToString() const = 0;

        bool IsInCategory(EventCategory category) const { return (GetCategoryFlags() & category) != EventCategory_None; }

    protected:
        bool m_IsHandled{ false };
    };

} // namespace spieler