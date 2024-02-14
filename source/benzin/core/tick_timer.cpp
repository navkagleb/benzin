#include "benzin/config/bootstrap.hpp"
#include "benzin/core/tick_timer.hpp"

namespace benzin
{

    void TickTimer::Reset()
    {
        m_PreviousTimePoint = std::chrono::high_resolution_clock::now();
    }

    void TickTimer::Tick()
    {
        m_CurrentTimePoint = std::chrono::high_resolution_clock::now();

        m_DeltaTime = ToUS(m_CurrentTimePoint - m_PreviousTimePoint);
        if (m_DeltaTime < std::chrono::microseconds::zero())
        {
            m_DeltaTime = std::chrono::microseconds::zero();
        }

        m_PreviousTimePoint = m_CurrentTimePoint;

        m_ElapsedTime += ToMS(m_DeltaTime);
    }

} // namespace benzin
