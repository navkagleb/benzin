#pragma once

namespace benzin
{

    class ScopedTimer
    {
    public:
        using Callback = std::function<void(std::chrono::microseconds us)>;

    public:
        explicit ScopedTimer(Callback&& callback);
        ~ScopedTimer();

        void ForceDestroy() const;

    private:
        const std::chrono::high_resolution_clock::time_point m_StartTimePoint;
        const Callback m_Callback;

        mutable bool m_IsForceDestoyed = false;
    };

    class ScopedLogTimer : public ScopedTimer
    {
    public:
        template <typename... Args>
        ScopedLogTimer(std::format_string<Args...> fmt, Args&&... args)
            : ScopedLogTimer{ std::format(fmt, std::forward<Args>(args)...) }
        {}

        explicit ScopedLogTimer(std::string&& scopeName);
    };

    class ScopedGrabTimer : public ScopedTimer
    {
    public:
        explicit ScopedGrabTimer(std::chrono::microseconds& outUS);
    };

    template <typename FunctionT>
    auto ProfileFunction(FunctionT&& function)
    {
        std::chrono::microseconds takedTime;

        if constexpr (std::is_void_v<decltype(function())>)
        {
            {
                const ScopedGrabTimer timer{ takedTime };
                function();
            }

            return takedTime;
        }
        else
        {
            const ScopedGrabTimer timer{ takedTime };
            decltype(auto) functionResult = function();
            timer.ForceDestroy();

            return std::make_pair(takedTime, std::move(functionResult));
        }
    }

} // namespace benzin

#define BenzinLogTimeOnScopeExit(...) const ::benzin::ScopedLogTimer BenzinUniqueVariableName(ScopedLogTimer){ __VA_ARGS__ }
#define BenzinGrabTimeOnScopeExit(outTime) const ::benzin::ScopedGrabTimer BenzinUniqueVariableName(ScopedGrabTimer){ outTime }

#define BenzinProfileFunction(function) ProfileFunction([&]{ return function; })
