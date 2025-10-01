#include "benzin/config/bootstrap.hpp"
#include "benzin/core/scoped_timer.hpp"

#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    static void LogScopeTime(std::string_view scopeName, std::chrono::microseconds time)
    {
        BenzinTrace("{} ({:.3f}ms)", scopeName, ToFloatMs(time), ToFloatSec(time));
    }

    static void GrabScopeTime(std::chrono::microseconds time, std::chrono::microseconds& outTime)
    {
        outTime = time;
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
        const auto time = ToUs(endTimePoint - m_StartTimePoint);
        m_Callback(time);
    }

    void ScopedTimer::ForceDestroy() const
    {
        this->~ScopedTimer();
        m_IsForceDestoyed = true;
    }

    // ScopedLogTimer

    ScopedLogTimer::ScopedLogTimer(std::string&& scopeName)
        : ScopedTimer{ [scopeName](std::chrono::microseconds time) { LogScopeTime(scopeName, time); } }
    {}

    // ScopedGrabTimer

    ScopedGrabTimer::ScopedGrabTimer(std::chrono::microseconds& outTime)
        : ScopedTimer{ [&outTime](std::chrono::microseconds time){ GrabScopeTime(time, outTime); } }
    {}

} // namespace benzin
