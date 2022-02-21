#pragma once

#include <cstdint>

namespace Spieler
{

    class Timer
    {
    public:
        Timer();

    public:
        inline float GetDeltaTime() const { return m_DeltaTime; }

        float GetTotalTime() const;

    public:
        void Start();
        void Stop();
        void Reset();
        void Tick();

    private:
        const float     m_SecondsPerCount   = 0.0;

    private:
        float           m_DeltaTime         = -1.0;

        std::int64_t    m_BaseTime          = 0;
        std::int64_t    m_PausedTime        = 0;
        std::int64_t    m_StopTime          = 0;
        std::int64_t    m_PreviousTime      = 0;
        std::int64_t    m_CurrentTime       = 0;

        bool            m_IsStopped         = false;
    };

} // namespace Spieler