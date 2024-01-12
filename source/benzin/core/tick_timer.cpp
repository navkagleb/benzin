#include "benzin/config/bootstrap.hpp"
#include "benzin/core/tick_timer.hpp"

namespace benzin
{

    namespace
    {

        uint64_t GetCounts()
        {
            // Counts - measure unit

            uint64_t result;
            BenzinAssert(::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&result)) != 0);

            return result;
        }

        uint64_t GetFrequency()
        {
            // Frequency - Counts per Second

            uint64_t result;
            BenzinAssert(::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&result)) != 0);

            return result;
        }

        const auto g_InverseFrequency = 1.0f / (float)GetFrequency();

    } // anonymous namespace

    MilliSeconds TickTimer::GetElapsedTime() const
    {
        return BenzinAsSeconds(((m_IsPaused ? m_PauseCounts : m_CurrentCounts) - m_PausedCounts - m_ResetCounts) * g_InverseFrequency);
    }

    void TickTimer::Continue()
    {
        if (!m_IsPaused)
        {
            return;
        }

        m_IsPaused = false;

        const uint64_t continueCounts = GetCounts();
        m_PausedCounts += continueCounts - m_PausedCounts;
        m_PreviousCounts = continueCounts;

        m_PauseCounts = 0;
    }

    void TickTimer::Pause()
    {
        if (m_IsPaused)
        {
            return;
        }

        m_IsPaused = true;
        m_PauseCounts = GetCounts();
    }

    void TickTimer::Reset()
    {
        const uint64_t currentCounts = GetCounts();

        m_ResetCounts = currentCounts;
        m_PreviousCounts = currentCounts;
        m_PauseCounts = 0;
        m_IsPaused = false;
    }

    void TickTimer::Tick()
    {
        if (m_IsPaused)
        {
            m_DeltaTime = MilliSeconds::zero();
            return;
        }   

        m_CurrentCounts = GetCounts();
        m_DeltaTime = BenzinAsSeconds((m_CurrentCounts - m_PreviousCounts) * g_InverseFrequency);
        m_PreviousCounts = m_CurrentCounts;

        if (m_DeltaTime < MilliSeconds::zero())
        {
            m_DeltaTime = MilliSeconds::zero();
        }
    }

} // namespace benzin
