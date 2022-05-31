#include "spieler/config/bootstrap.hpp"

#include "spieler/core/timer.hpp"

namespace spieler
{

    namespace _internal
    {

        // Counts - measure unit
        uint64_t GetCounts()
        {
            uint64_t result{};

            return ::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&result)) ? result : -1;
        }

        // Frequency - Counts per Second
        uint64_t GetFrequency()
        {
            uint64_t result{};

            return ::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&result)) ? result : -1;
        }

    } // namespace _internal

    Timer::Timer()
        : m_SecondsPerCount(1.0f / static_cast<float>(_internal::GetFrequency()))
    {}

    float Timer::GetTotalTime() const
    {
        return static_cast<float>((m_IsStopped ? m_StopTime : m_CurrentTime) - m_PausedTime - m_BaseTime) * m_SecondsPerCount;
    }

    void Timer::Start() 
    {
        const uint64_t startTime{ _internal::GetCounts() };

        if (m_IsStopped)
        {
            m_PausedTime += (startTime - m_StopTime);
            m_PreviousTime = startTime;

            m_StopTime = 0;
            m_IsStopped = false;
        }
    }

    void Timer::Stop()
    {
        if (!m_IsStopped)
        {
            m_StopTime = _internal::GetCounts();
            m_IsStopped = true;
        }
    }

    void Timer::Reset()
    {
        const uint64_t currentTime{ _internal::GetCounts() };

        m_BaseTime = currentTime;
        m_PreviousTime = currentTime;
        m_StopTime = 0;
        m_IsStopped = false;
    }

    void Timer::Tick()
    {
        if (m_IsStopped)
        {
            m_DeltaTime = 0.0f;
            return;
        }

        m_CurrentTime = _internal::GetCounts();
        m_DeltaTime = static_cast<float>(m_CurrentTime - m_PreviousTime) * m_SecondsPerCount;
        m_PreviousTime = m_CurrentTime;

        if (m_DeltaTime < 0.0f)
        {
            m_DeltaTime = 0.0f;
        }
    }

} // namespace spieler