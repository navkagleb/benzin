#include "benzin/config/bootstrap.hpp"
#include "benzin/core/tick_timer.hpp"

#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    float TickTimer::GetDeltaTimeInMs() const
    {
        return benzin::ToFloatMs(m_DeltaTime);
    }

    float TickTimer::GetDeltaTimeInSec() const
    {
        return benzin::ToFloatSec(m_DeltaTime);
    }

    float TickTimer::GetElapsedTimeInSec() const
    {
        return benzin::MsToFloatSec(m_ElapsedTimeInMs);
    }

    void TickTimer::SetPaused(bool isPaused)
    {
        m_IsPaused = isPaused;

        if (!m_IsPaused)
        {
            m_PreviousTimePoint = std::chrono::high_resolution_clock::now();
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

        m_DeltaTime = ToUs(m_CurrentTimePoint - std::exchange(m_PreviousTimePoint, m_CurrentTimePoint));;
        if (m_DeltaTime < std::chrono::microseconds::zero())
        {
            m_DeltaTime = std::chrono::microseconds::zero();
        }

        m_ElapsedTimeInMs += benzin::ToFloatMs(m_DeltaTime);
    }

}
