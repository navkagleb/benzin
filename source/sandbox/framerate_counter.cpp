#include "sandbox/bootstrap.hpp"
#include "sandbox/framerate_counter.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/utility/time_utils.hpp>

namespace sandbox
{

    static const std::chrono::microseconds g_UpdateInterval = benzin::SecToUs(1.0f);

    bool FrameRateCounter::IsIntervalPassed() const
    {
        return m_ElapsedTime >= g_UpdateInterval;
    }

    void FrameRateCounter::OnUpdate(const benzin::TickTimer& tickTimer)
    {
        m_ElapsedTime += tickTimer.GetDeltaTime();
        m_ElapsedFrameCount++;
    }

    void FrameRateCounter::UpdateFrameRate()
    {
        BenzinAssert(IsIntervalPassed());

        m_FrameRate = m_ElapsedFrameCount / benzin::ToFloatSec(m_ElapsedTime);

        m_ElapsedTime -= g_UpdateInterval;
        m_ElapsedFrameCount = 0;
    }

}
