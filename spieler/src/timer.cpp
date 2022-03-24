#include "timer.hpp"

#include <Windows.h>

namespace Spieler
{

    namespace Internal
    {

        // Counts - measure unit
        std::int64_t GetCounts()
        {
            std::int64_t result{};

            const bool status = QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&result));

            return status ? result : -1;
        }

        // Frequency - Counts per Second
        std::int64_t GetFrequency()
        {
            std::int64_t result{};

            const bool status =  QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&result));

            return status ? result : -1;
        }

    } // namespace Internal

    Timer::Timer()
        : m_SecondsPerCount(1.0f / static_cast<float>(Internal::GetFrequency()))
    {}

    float Timer::GetTotalTime() const
    {
        return static_cast<float>((m_IsStopped ? m_StopTime : m_CurrentTime) - m_PausedTime - m_BaseTime) * m_SecondsPerCount;
    }

    void Timer::Start() 
    {
        std::int64_t startTime = Internal::GetCounts();

        if (m_IsStopped)
        {
            m_PausedTime   += (startTime - m_StopTime);
            m_PreviousTime  = startTime;

            m_StopTime      = 0;
            m_IsStopped     = false;
        }
    }

    void Timer::Stop()
    {
        if (!m_IsStopped)
        {
            m_StopTime  = Internal::GetCounts();
            m_IsStopped = true;
        }
    }

    void Timer::Reset()
    {
        std::int64_t currentTime;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentTime));

        m_BaseTime      = currentTime;
        m_PreviousTime  = currentTime;
        m_StopTime      = 0;
        m_IsStopped     = false;
    }

    void Timer::Tick()
    {
        if (m_IsStopped)
        {
            m_DeltaTime = 0.0f;
            return;
        }

        m_CurrentTime   = Internal::GetCounts();
        m_DeltaTime     = static_cast<float>(m_CurrentTime - m_PreviousTime) * m_SecondsPerCount;
        m_PreviousTime  = m_CurrentTime;

        if (m_DeltaTime < 0.0f)
        {
            m_DeltaTime = 0.0f;
        }
    }

} // namespace Spieler