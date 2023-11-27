#pragma once

namespace benzin
{

    enum class EventType : uint8_t
    {
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
    
    enum class EventCategoryFlag : int8_t
    {
        Window,
        Input,
        Keyboard,
        Mouse,
        MouseButton,
    };
    using EventCategoryFlags = magic_enum::containers::bitset<EventCategoryFlag>;
    static_assert(sizeof(EventCategoryFlags) <= sizeof(EventCategoryFlag));

    class Event
    {
    public:
        friend class EventDispatcher;
        friend class ImGuiLayer;

    public:
        virtual ~Event() = default;

    public:
        bool IsHandled() const { return m_IsHandled; }

        virtual EventType GetEventType() const = 0;
        virtual bool IsInCategory(EventCategoryFlag flag) const = 0;

    protected:
        bool m_IsHandled = false;
    };

    template <EventType EventTypeValue, EventCategoryFlag... Flags>
    class EventInfo : public Event
    {
    public:
        static EventType GetStaticEventType() { return EventTypeValue; }

        static const EventCategoryFlags& GetStaticEventCategoryFlags()
        {
            static EventCategoryFlags flags{ Flags... };
            return flags;
        }

    public:
        EventType GetEventType() const override { return GetStaticEventType(); }
        bool IsInCategory(EventCategoryFlag flag) const override { return GetStaticEventCategoryFlags()[flag]; }
    };

    class EventDispatcher
    {
    public:
        explicit EventDispatcher(Event& event)
            : m_Event{ event }
        {}

    public:
        template <std::derived_from<Event> EventChild>
        bool Dispatch(const std::function<bool(EventChild&)>& callback)
        {
            if (m_Event.GetEventType() == EventChild::GetStaticEventType())
            {
                m_Event.m_IsHandled = callback((EventChild&)m_Event);
                return true;
            }

            return false;
        }

        template <std::derived_from<Event> EventChild, typename ClassType>
        bool Dispatch(bool (ClassType::*MemberCallback)(EventChild&), ClassType& classInstance)
        {
            return Dispatch<EventChild>([&](EventChild& event)
            {
                return std::invoke_r<bool>(MemberCallback, classInstance, event);
            });
        }

    private:
        Event& m_Event;
    };

} // namespace benzin
