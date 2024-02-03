#pragma once

namespace benzin
{

    class ScopedLogTimer
    {
    public:
        explicit ScopedLogTimer(std::string&& scopeName);
        ~ScopedLogTimer();

    private:
        const std::string m_ScopeName;
        const std::chrono::high_resolution_clock::time_point m_StartTimePoint;
    };

} // namespace benzin

#define BenzinLogTimeOnScopeExit(scopeName) ::benzin::ScopedLogTimer BenzinUniqueVariableName(ScopedLogTimer){ scopeName }
