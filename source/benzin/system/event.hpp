#pragma once

#include "benzin/core/enum_flags.hpp"

namespace benzin
{

    enum class EventType : uint8_t
    {
        WindowCloseEvent,
        WindowMaximizedEvent,
        WindowMinimizedEvent,
        WindowRestoredEvent,
        WindowFocusedEvent,
        WindowUnfocusedEvent,
        WindowEnterResizingEvent,
        WindowExitResizingEvent,
        WindowResizingEvent,
        WindowResizedEvent,

        MouseMovedEvent,
        MouseScrolledEvent,
        MouseButtonPressedEvent,
        MouseButtonReleasedEvent,

        KeyPressedEvent,
        KeyReleasedEvent,
        KeyTypedEvent,
    };
    
    enum class EventCategoryFlag : uint8_t
    {
        Window,
        Input,
        Keyboard,
        Mouse,
        MouseButton,
    };
    BenzinEnableFlagsForEnum(EventCategoryFlag);

    class Event
    {
    public:
        friend class EventDispatcher;
        friend class ImGuiManager;

    public:
        virtual ~Event() = default;

        auto IsHandled() const { return m_IsHandled; }

        virtual EventType GetEventType() const = 0;
        virtual bool IsInCategory(EventCategoryFlag flag) const = 0;

    private:
        bool m_IsHandled = false;
    };

    template <EventType T>
    class EventInfo : public Event
    {
    public:
        static auto GetStaticEventType()
        {
            return T;
        }

        EventInfo(EnumFlags<EventCategoryFlag> flags)
            : m_CategoryFlags{ flags }
        {}

        EventType GetEventType() const override
        {
            return GetStaticEventType();
        }

        bool IsInCategory(EventCategoryFlag flag) const override
        {
            return m_CategoryFlags.IsSet(flag);
        }

    private:
        EnumFlags<EventCategoryFlag> m_CategoryFlags;
    };

    class EventDispatcher
    {
    public:
        using EventNoParamCallback = std::function<bool()>;

        template <std::derived_from<Event> EventT>
        using EventParamCallback = std::function<bool(const EventT&)>;

        explicit EventDispatcher(Event& event)
            : m_Event{ event }
        {}

        template <std::derived_from<Event> EventT>
        bool ForceDispatch(const EventNoParamCallback& callback) const
        {
            if (m_Event.GetEventType() == EventT::GetStaticEventType())
            {
                m_Event.m_IsHandled = callback();
                return true;
            }

            return false;
        }

        template <std::derived_from<Event> EventT>
        bool ForceDispatch(const EventParamCallback<EventT>& callback) const
        {
            if (m_Event.GetEventType() == EventT::GetStaticEventType())
            {
                m_Event.m_IsHandled = callback((const EventT&)m_Event);
                return true;
            }

            return false;
        }

        template <std::derived_from<Event> EventT, typename ClassT>
        bool ForceDispatch(bool (ClassT::* MemberCallback)(const EventT&), ClassT* classInstance) const
        {
            return ForceDispatch<EventT>([&](const EventT& event)
            {
                return std::invoke_r<bool>(MemberCallback, classInstance, event);
            });
        }

        template <std::derived_from<Event> EventT>
        bool Dispatch(const EventNoParamCallback& callback) const
        {
            if (m_Event.IsHandled())
            {
                return false;
            }

            return ForceDispatch<EventT>(callback);
        }

        template <std::derived_from<Event> EventT>
        bool Dispatch(const EventParamCallback<EventT>& callback) const
        {
            if (m_Event.IsHandled())
            {
                return false;
            }

            return ForceDispatch<EventT>(callback);
        }

        template <std::derived_from<Event> EventT, typename ClassT>
        bool Dispatch(bool (ClassT::*MemberCallback)(const EventT&), ClassT* classInstance) const
        {
            return Dispatch<EventT>([&](const EventT& event)
            {
                return std::invoke_r<bool>(MemberCallback, classInstance, event);
            });
        }

    private:
        Event& m_Event;
    };

} // namespace benzin
