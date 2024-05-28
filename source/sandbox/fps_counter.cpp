#include "sandbox/bootstrap.hpp"
#include "sandbox/fps_counter.hpp"

#include <benzin/core/tick_timer.hpp>

namespace sandbox
{

    void FpsCounter::TickFrame(const benzin::TickTimer& frameTimer)
    {
        m_ElapsedTime += frameTimer.GetDeltaTime();
        m_ElapsedFrameCount++;
    }

    void FpsCounter::UpdateFps(std::chrono::microseconds interval)
    {
        m_Fps = m_ElapsedFrameCount / benzin::ToFloatSec(m_ElapsedTime);

        m_ElapsedTime -= interval;
        m_ElapsedFrameCount = 0;
    }

}
