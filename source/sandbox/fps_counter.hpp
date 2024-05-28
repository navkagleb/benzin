#pragma once

#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    class TickTimer;

}

namespace sandbox
{

    class FpsCounter
    {
    public:
        auto GetFps() const { return m_Fps; }
        auto GetDeltaTime() const { return benzin::SecToUs(1.0f / m_Fps); }

        void TickFrame(const benzin::TickTimer& frameTimer);
        void UpdateFps(std::chrono::microseconds interval);

    private:
        std::chrono::microseconds m_ElapsedTime{};
        uint32_t m_ElapsedFrameCount = 0;

        float m_Fps = 0.0f;
    };

}
