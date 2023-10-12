#include "benzin/config/bootstrap.hpp"
#include "benzin/core/timer.hpp"

namespace benzin
{

    namespace
    {

        // Counts - measure unit
        uint64_t GetCounts()
        {
            uint64_t result;
            return ::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&result)) ? result : -1;
        }

        // Frequency - Counts per Second
        uint64_t GetFrequency()
        {
            uint64_t result;
            return ::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&result)) ? result : -1;
        }

    } // namespace _internal

    Timer::Timer()
        : m_SecondsPerCount{ 1.0f / static_cast<float>(GetFrequency()) }
    {}

    float Timer::GetTotalTime() const
    {
        return static_cast<float>((m_IsStopped ? m_StopTime : m_CurrentTime) - m_PausedTime - m_BaseTime) * m_SecondsPerCount;
    }

    void Timer::Start() 
    {
        const uint64_t startTime = GetCounts();

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
            m_StopTime = GetCounts();
            m_IsStopped = true;
        }
    }

    void Timer::Reset()
    {
        const uint64_t currentTime = GetCounts();

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

        m_CurrentTime = GetCounts();
        m_DeltaTime = static_cast<float>(m_CurrentTime - m_PreviousTime) * m_SecondsPerCount;
        m_PreviousTime = m_CurrentTime;

        if (m_DeltaTime < 0.0f)
        {
            m_DeltaTime = 0.0f;
        }
    }

} // namespace benzin
