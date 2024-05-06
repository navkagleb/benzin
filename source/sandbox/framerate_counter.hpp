#pragma once

#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    class TickTimer;

}

namespace sandbox
{

    class FrameRateCounter
    {
    public:
        auto GetFrameRate() const { return m_FrameRate; }
        auto GetDeltaTime() const { return benzin::SecToUs(1.0f / m_FrameRate); }

        bool IsIntervalPassed() const;

        void OnUpdate(const benzin::TickTimer& tickTimer);
        void UpdateFrameRate();

    private:
        float m_FrameRate = 0.0f;

        std::chrono::microseconds m_ElapsedTime = std::chrono::microseconds::zero();
        uint32_t m_ElapsedFrameCount = 0;
    };

}
