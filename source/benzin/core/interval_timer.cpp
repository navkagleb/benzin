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
        m_AccumulatedFrameCount++;

        if (m_AccumulatedInterval >= m_Interval)
        {
            for (const auto& callback : m_Callbacks)
            {
                callback(m_AccumulatedFrameCount);
            }

            m_AccumulatedInterval -= m_Interval;
            m_AccumulatedFrameCount = 0;
        }
    }

    void IntervalTimer::PushCallback(Callback&& callback)
    {
        m_Callbacks.push_back(std::move(callback));
    }

}
