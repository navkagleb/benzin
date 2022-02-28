#pragma once

#include <concepts>

#include "event.h"

namespace Spieler
{

    namespace
    {

        template <typename T>
        concept EventChild = std::is_base_of_v<Event, T>;

    }

#define SPIELER_BIND_EVENT_CALLBACK(callback) [this]<typename T>(T& event) { return this->callback(event); }

    class EventDispatcher
    {
    public:
        template <EventChild T>
        using EventCallbackFunction = std::function<bool(T&)>;

    public:
        explicit EventDispatcher(Event& event) 
            : m_Event(event) 
        {}
    
    public:
        template <EventChild T>
        bool Dispatch(EventCallbackFunction<T> callback)
        {
            if (m_Event.GetEventType() == T::GetStaticEventType())
            {
                m_Event.m_Handled = callback(static_cast<T&>(m_Event));
                return true;
            }

            return false;
        }

    private:
        Event& m_Event;
    };

} // namespace Spieler