#pragma once

#include "benzin/core/timer.hpp"

namespace benzin
{

    class Event;

    class FrameStats
    {
    public:
        BenzinDefineNonCopyable(FrameStats);
        BenzinDefineNonMoveable(FrameStats);

    public:
        FrameStats() = default;

    public:
        float GetElapsedIntervalInSeconds(float seconds) const { return m_ElapsedIntervalInSeconds; }
        void SetElapsedIntervalInSeconds(float seconds) { m_ElapsedIntervalInSeconds = seconds; }

        float GetFrameRate() const { return m_FrameRate; }
        float GetDeltaTimeMS() const { return 1000.0f / m_FrameRate; }

        bool IsReady() const { return m_ElapsedTimeInSeconds >= m_ElapsedIntervalInSeconds; }

        void OnEndFrame()
        {
            if (IsReady())
            {
                m_FrameRate = static_cast<float>(m_ElapsedFrameCount) / m_ElapsedTimeInSeconds;

                m_ElapsedFrameCount = 0;
                m_ElapsedTimeInSeconds -= m_ElapsedIntervalInSeconds;
            }
        }

        void OnUpdate(float dt)
        {
            m_ElapsedFrameCount++;
            m_ElapsedTimeInSeconds += dt;
        }

    private:
        float m_ElapsedIntervalInSeconds = 1.0f;

        uint32_t m_ElapsedFrameCount = 0;
        float m_ElapsedTimeInSeconds = 0.0f;
        float m_FrameRate = 0.0f;
    };

    class Layer
    {
    public:
        static inline Timer s_FrameTimer;
        static inline FrameStats s_FrameStats;

    public:
        static void OnStaticBeforeMainLoop()
        {
            s_FrameTimer.Reset();
        }

        static void OnStaticEndFrame()
        {
            s_FrameStats.OnEndFrame();
        }

        static void OnStaticUpdate()
        {
            s_FrameTimer.Tick();
            s_FrameStats.OnUpdate(s_FrameTimer.GetDeltaTimeInSeconds());
        }

    public:
        virtual ~Layer() = default;

    public:
        virtual void OnBeginFrame() {}
        virtual void OnEndFrame() {}

        virtual void OnEvent(Event& event) {}
        virtual void OnUpdate() {}
        virtual void OnRender() {}
        virtual void OnImGuiRender() {}
    };

} // namespace benzin
