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
            BenzinAssert(::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&result)) != 0);

            return result;
        }

        // Frequency - Counts per Second
        uint64_t GetFrequency()
        {
            uint64_t result;
            BenzinAssert(::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&result)) != 0);

            return result;
        }

        const auto g_InverseFrequency = 1.0f / static_cast<float>(GetFrequency());

    } // anonymous namespace

    void Timer::Start() 
    {
        if (!m_IsStopped)
        {
            return;
        }

        const uint64_t startTime = GetCounts();

        m_PausedTime += (startTime - m_StopTime);
        m_PreviousTime = startTime;

        m_StopTime = 0;
        m_IsStopped = false;
    }

    void Timer::Stop()
    {
        if (m_IsStopped)
        {
            return;
        }

        m_StopTime = GetCounts();
        m_IsStopped = true;
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
            m_DeltaTime = MilliSeconds::zero();
            return;
        }   

        m_CurrentTime = GetCounts();
        m_DeltaTime = ToMS(static_cast<float>(m_CurrentTime - m_PreviousTime) * g_InverseFrequency);
        m_PreviousTime = m_CurrentTime;

        if (m_DeltaTime < MilliSeconds::zero())
        {
            m_DeltaTime = MilliSeconds::zero();
        }
    }

} // namespace benzin
