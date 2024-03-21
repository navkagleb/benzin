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

    template <EventType T>
    class EventInfo : public Event
    {
    public:
        static auto GetStaticEventType()
        {
            return T;
        }

        EventInfo(EventCategoryFlags flags)
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
        EventCategoryFlags m_CategoryFlags;
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
