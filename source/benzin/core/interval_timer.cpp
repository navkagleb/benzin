#include "benzin/config/bootstrap.hpp"
#include "benzin/core/interval_timer.hpp"

#include "benzin/core/tick_timer.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    IntervalTimer::IntervalTimer(const TickTimer& baseTimer, std::chrono::microseconds interval)
        : m_BaseTimer{ baseTimer }
        , m_Interval{ interval }
    {}

    void IntervalTimer::AccumulateInterval()
    {
        m_AccumulatedInterval += m_BaseTimer.GetDeltaTime();
        m_AccumulatedFrameCount++;

        if (m_AccumulatedInterval >= m_Interval)
        {
            for (const auto& callback : m_Callbacks)
            {
                const float timeInMs = ToFloatMs(m_AccumulatedInterval);
                callback(timeInMs, m_AccumulatedFrameCount);
            }

            m_AccumulatedInterval = std::chrono::microseconds::zero();
            m_AccumulatedFrameCount = 0;
        }
    }

    void IntervalTimer::AddCallback(Callback&& callback) const
    {
        m_Callbacks.push_back(std::move(callback));
    }

}
