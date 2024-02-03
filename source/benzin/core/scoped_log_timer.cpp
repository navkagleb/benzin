#include "benzin/config/bootstrap.hpp"
#include "benzin/core/scoped_log_timer.hpp"

namespace benzin
{

    ScopedLogTimer::ScopedLogTimer(std::string&& scopeName)
        : m_ScopeName{ std::move(scopeName) }
        , m_StartTimePoint{ std::chrono::high_resolution_clock::now() }
    {}

    ScopedLogTimer::~ScopedLogTimer()
    {
        const auto endTimePoint = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTimePoint - m_StartTimePoint);

        const auto us = (float)duration.count();
        const auto ms = us / 1000.0f;
        const auto s = ms / 1000.0f;

        BenzinTrace("Scope '{}' takes {:.3f}ms, {:.3f}s", m_ScopeName, ms, s);
    }

} // namespace benzin
