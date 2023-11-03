#pragma once

namespace benzin
{

    class Timer
    {
    public:
        Seconds GetDeltaTime() const { return m_DeltaTime; }

    public:
        void Start();
        void Stop();
        void Reset();
        void Tick();

    private:
        uint64_t m_BaseTime = 0;
        uint64_t m_PausedTime = 0;
        uint64_t m_StopTime = 0;
        uint64_t m_PreviousTime = 0;
        uint64_t m_CurrentTime = 0;

        MilliSeconds m_DeltaTime = MilliSeconds::zero();

        bool m_IsStopped = true;
    };

} // namespace benzin
