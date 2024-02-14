#include "benzin/config/bootstrap.hpp"
#include "benzin/core/scoped_timer.hpp"

namespace benzin
{

    namespace
    {

        void LogScopeTime(std::string_view scopeName, std::chrono::microseconds us)
        {
            BenzinTrace("Scope '{}' takes {:.3f}ms, {:.3f}s", scopeName, ToFloatMS(us), ToFloatSec(us));
        }

        void GrabScopeTime(std::chrono::microseconds us, std::chrono::microseconds& outUS)
        {
            outUS = us;
        }

    } // anonymous namespace

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
        const auto us = ToUS(endTimePoint - m_StartTimePoint);
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
