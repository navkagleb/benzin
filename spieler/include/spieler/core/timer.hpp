#pragma once

namespace spieler
{

    class Timer
    {
    public:
        Timer();

    public:
        // Return in seconds
        float GetDeltaTime() const { return m_DeltaTime; }

        float GetTotalTime() const;

    public:
        void Start();
        void Stop();
        void Reset();
        void Tick();

    private:
        const float m_SecondsPerCount{ 0.0f };

    private:
        float m_DeltaTime{ -1.0f };

        uint64_t m_BaseTime{ 0 };
        uint64_t m_PausedTime{ 0 };
        uint64_t m_StopTime{ 0 };
        uint64_t m_PreviousTime{ 0 };
        uint64_t m_CurrentTime{ 0 };

        bool m_IsStopped{ false };
    };

} // namespace spieler