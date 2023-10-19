#pragma once

namespace benzin
{

    class Timer
    {
    public:
        float GetDeltaTimeInSeconds() const { return m_DeltaTimeInSeconds; }
        float GetTotalTimeInSeconds() const;

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

        float m_DeltaTimeInSeconds = -1.0f;

        bool m_IsStopped = true;
    };

} // namespace benzin
