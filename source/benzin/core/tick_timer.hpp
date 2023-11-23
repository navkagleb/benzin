#pragma once

namespace benzin
{

    class TickTimer
    {
    public:
        MilliSeconds GetDeltaTime() const { return m_DeltaTime; }
        MilliSeconds GetElapsedTime() const;

        bool IsPaused() const { return m_IsPaused; }

    public:
        void Continue();
        void Pause();

        void Reset();

        void Tick();

    private:
        uint64_t m_ResetCounts = 0;
        uint64_t m_PauseCounts = 0;
        uint64_t m_PausedCounts = 0;

        uint64_t m_PreviousCounts = 0;
        uint64_t m_CurrentCounts = 0;

        MilliSeconds m_DeltaTime = MilliSeconds::zero();

        bool m_IsPaused = true;
    };

} // namespace benzin
