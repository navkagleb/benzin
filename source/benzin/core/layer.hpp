#pragma once

#include "benzin/core/tick_timer.hpp"

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
        Seconds GetElapsedInterval() const { return m_ElapsedInterval; }
        void SetElapsedIntervalInSeconds(Seconds seconds) { m_ElapsedInterval = seconds; }

        float GetFrameRate() const { return m_FrameRate; }
        MilliSeconds GetDeltaTime() const { return MilliSeconds{ 1000.0f / m_FrameRate}; }

        bool IsReady() const { return m_ElapsedTime >= m_ElapsedInterval; }

        void OnEndFrame()
        {
            if (IsReady())
            {
                m_FrameRate = static_cast<float>(m_ElapsedFrameCount) / m_ElapsedTime.count();

                m_ElapsedTime -= m_ElapsedInterval;
                m_ElapsedFrameCount = 0;
            }
        }

        void OnUpdate(MilliSeconds dt)
        {
            m_ElapsedTime += dt;
            m_ElapsedFrameCount++;
        }

    private:
        Seconds m_ElapsedInterval{ 1.0f };

        Seconds m_ElapsedTime = Seconds::zero();
        uint32_t m_ElapsedFrameCount = 0;

        float m_FrameRate = 0.0f;
    };

    class Layer
    {
    public:
        static inline TickTimer s_FrameTimer;
        static inline FrameStats s_FrameStats;

    public:
        static void OnStaticBeforeMainLoop()
        {
            s_FrameTimer.Reset();
        }

        static void OnStaticAfterMainLoop()
        {
            const auto elapsedTime = s_FrameTimer.GetElapsedTime();

            BenzinTrace("ExecutionTime: {} ({})", BenzinAsSeconds(elapsedTime), elapsedTime);
        }

        static void OnStaticBeginFrame() {}

        static void OnStaticEndFrame()
        {
            s_FrameStats.OnEndFrame();
        }

        static void OnStaticUpdate()
        {
            s_FrameTimer.Tick();
            s_FrameStats.OnUpdate(s_FrameTimer.GetDeltaTime());
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
