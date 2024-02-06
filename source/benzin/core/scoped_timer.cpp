#include "benzin/config/bootstrap.hpp"
#include "benzin/core/scoped_timer.hpp"

namespace benzin
{

    namespace
    {

        void LogScopeTime(std::string_view scopeName, std::chrono::microseconds us)
        {
            const auto ms = us.count() / 1000.0f;
            const auto s = ms / 1000.0f;

            BenzinTrace("Scope '{}' takes {:.3f}ms, {:.3f}s", scopeName, ms, s);
        }

    } // anonymous namespace

    // ScopedTimer

    ScopedTimer::ScopedTimer(Callback callback)
        : m_StartTimePoint{ std::chrono::high_resolution_clock::now() }
        , m_Callback{ callback }
    {
        BenzinAssert((bool)callback);
    }

    ScopedTimer::~ScopedTimer()
    {
        if (m_IsForceDestoyed)
        {
            return;
        }

        const auto endTimePoint = std::chrono::high_resolution_clock::now();
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(endTimePoint - m_StartTimePoint);

        m_Callback(us);
    }

    void ScopedTimer::ForceDestroy() const
    {
        this->~ScopedTimer();
        m_IsForceDestoyed = true;
    }

    // ScopedLogTimer

    ScopedLogTimer::ScopedLogTimer(std::string&& scopeName)
        : m_ScopeName{ std::move(scopeName) }
        , m_ScopedTimer{ [this](std::chrono::microseconds us){ LogScopeTime(m_ScopeName, us); } }
    {}

} // namespace benzin
