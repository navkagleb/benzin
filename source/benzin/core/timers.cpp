#include <benzin/config/bootstrap.hpp>
#include <benzin/core/timers.hpp>

namespace benzin
{

    // TickTimer

    void TickTimer::SetPaused(bool isPaused)
    {
        m_IsPaused = isPaused;

        if (!m_IsPaused)
        {
            Reset();
            Tick();
        }
    }

    void TickTimer::Reset()
    {
        m_PreviousTimePoint = std::chrono::high_resolution_clock::now();
    }

    void TickTimer::Tick()
    {
        if (m_IsPaused)
            return;
    
        m_CurrentTimePoint = std::chrono::high_resolution_clock::now();
        m_DeltaTime = m_CurrentTimePoint - m_PreviousTimePoint;
        m_PreviousTimePoint = m_CurrentTimePoint;
        m_ElapsedTimeInSec += m_DeltaTime.count() / 1000.0f / 1000.0f / 1000.0f;
    }

    // IntervalTimer

    IntervalTimer::IntervalTimer(const TickTimer& baseTimer, std::chrono::microseconds interval)
        : m_BaseTimer{ baseTimer }
        , m_Interval{ interval }
    {}

    void IntervalTimer::AccumulateInterval()
    {
        m_AccumulatedInterval += std::chrono::duration_cast<std::chrono::microseconds>(m_BaseTimer.GetDeltaTime());
        m_AccumulatedFrameCount++;

        if (m_AccumulatedInterval >= m_Interval)
        {
            for (const auto& callback : m_Callbacks)
            {
                const float timeInMs = m_AccumulatedInterval.count() / 1000.0f;
                callback(timeInMs, m_AccumulatedFrameCount);
            }

            m_AccumulatedInterval = {};
            m_AccumulatedFrameCount = 0;
        }
    }

    void IntervalTimer::AddCallback(Callback&& callback) const
    {
        m_Callbacks.push_back(std::move(callback));
    }

}
