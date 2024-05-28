#include "benzin/config/bootstrap.hpp"
#include "benzin/core/interval_timer.hpp"

#include "benzin/core/tick_timer.hpp"

namespace benzin
{

    IntervalTimer::IntervalTimer(std::chrono::microseconds interval)
        : m_Interval{ interval }
    {}

    void IntervalTimer::AccumulateInterval(const TickTimer& frameTimer)
    {
        m_AccumulatedInterval += frameTimer.GetDeltaTime();

        if (m_AccumulatedInterval >= m_Interval)
        {
            std::ranges::for_each(m_Callbacks, &Callback::operator());

            m_AccumulatedInterval -= m_Interval;
        }
    }

    void IntervalTimer::PushCallback(Callback&& callback)
    {
        m_Callbacks.push_back(std::move(callback));
    }

}
