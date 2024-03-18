#include "benzin/config/bootstrap.hpp"
#include "benzin/core/tick_timer.hpp"

#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    void TickTimer::Reset()
    {
        m_PreviousTimePoint = std::chrono::high_resolution_clock::now();
    }

    void TickTimer::Tick()
    {
        m_CurrentTimePoint = std::chrono::high_resolution_clock::now();

        m_DeltaTime = ToUs(m_CurrentTimePoint - m_PreviousTimePoint);
        if (m_DeltaTime < std::chrono::microseconds::zero())
        {
            m_DeltaTime = std::chrono::microseconds::zero();
        }

        m_PreviousTimePoint = m_CurrentTimePoint;

        m_ElapsedTime += ToMs(m_DeltaTime);
    }

} // namespace benzin
