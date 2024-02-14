#pragma once

namespace benzin
{

    class TickTimer
    {
    public:
        auto GetDeltaTime() const { return m_DeltaTime; }
        auto GetElapsedTime() const { return m_ElapsedTime; }

        void Reset();
        void Tick();

    private:
        std::chrono::high_resolution_clock::time_point m_CurrentTimePoint;
        std::chrono::high_resolution_clock::time_point m_PreviousTimePoint;

        std::chrono::microseconds m_DeltaTime;
        std::chrono::milliseconds m_ElapsedTime;
    };

} // namespace benzin
