#include "benzin/config/bootstrap.hpp"
#include "benzin/core/scoped_timer.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    static void LogScopeTime(std::string_view scopeName, std::chrono::microseconds us)
    {
        BenzinTrace("Scope '{}' takes {:.3f}ms, {:.3f}s", scopeName, ToFloatMs(us), ToFloatSec(us));
    }

    static void GrabScopeTime(std::chrono::microseconds us, std::chrono::microseconds& outUS)
    {
        outUS = us;
    }

    // ScopedTimer

    ScopedTimer::ScopedTimer(Callback&& callback)
        : m_StartTimePoint{ std::chrono::high_resolution_clock::now() }
        , m_Callback{ std::move(callback) }
    {
        BenzinAssert((bool)m_Callback);
    }

    ScopedTimer::~ScopedTimer()
    {
        if (m_IsForceDestoyed)
        {
            return;
        }

        const auto endTimePoint = std::chrono::high_resolution_clock::now();
        const auto us = ToUs(endTimePoint - m_StartTimePoint);
        m_Callback(us);
    }

    void ScopedTimer::ForceDestroy() const
    {
        this->~ScopedTimer();
        m_IsForceDestoyed = true;
    }

    // ScopedLogTimer

    ScopedLogTimer::ScopedLogTimer(std::string&& scopeName)
        : ScopedTimer{ [scopeName](std::chrono::microseconds us) { LogScopeTime(scopeName, us); } }
    {}

    // ScopedGrabTimer

    ScopedGrabTimer::ScopedGrabTimer(std::chrono::microseconds& outUS)
        : ScopedTimer{ [&outUS](std::chrono::microseconds us){ GrabScopeTime(us, outUS); } }
    {}

} // namespace benzin
