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

        void Reset();
        void Tick();

    private:
        std::chrono::high_resolution_clock::time_point m_CurrentTimePoint;
        std::chrono::high_resolution_clock::time_point m_PreviousTimePoint;

        std::chrono::microseconds m_DeltaTime{};
        float m_ElapsedTimeInMs = 0.0f;
    };

} // namespace benzin
