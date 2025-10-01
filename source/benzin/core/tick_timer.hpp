#pragma once

namespace benzin
{

    class TickTimer
    {
    public:
        auto GetDeltaTime() const { return m_DeltaTime; }
        auto GetElapsedTimeInMs() const { return m_ElapsedTimeInMs; }

        float GetDeltaTimeInMs() const;
        float GetDeltaTimeInSec() const;
        float GetElapsedTimeInSec() const;

        auto IsPaused() const { return m_IsPaused; }
        void SetPaused(bool isPaused);

        void Reset();
        void Tick();

    private:
        using Clock = std::chrono::high_resolution_clock;

        Clock::time_point m_CurrentTimePoint = {};
        Clock::time_point m_PreviousTimePoint = {};

        std::chrono::microseconds m_DeltaTime = {};
        float m_ElapsedTimeInMs = 0.0f;

        bool m_IsPaused = false;
    };

}
