#pragma once

namespace benzin
{

    class ScopedTimer
    {
    public:
        using Callback = std::function<void(std::chrono::microseconds us)>;

    public:
        explicit ScopedTimer(Callback callback);
        ~ScopedTimer();

        void ForceDestroy() const;

    private:
        const std::chrono::high_resolution_clock::time_point m_StartTimePoint;
        const Callback m_Callback;

        mutable bool m_IsForceDestoyed = false;
    };

    class ScopedLogTimer
    {
    public:
        template <typename... Args>
        ScopedLogTimer(std::format_string<Args...> format, Args&&... args)
            : ScopedLogTimer{ std::vformat(format.get(), std::make_format_args(args...)) }
        {}

        explicit ScopedLogTimer(std::string&& scopeName);

    private:
        const std::string m_ScopeName;
        const ScopedTimer m_ScopedTimer;
    };

    template <typename FunctionT>
    auto ProfileFunction(FunctionT&& function)
    {
        std::chrono::microseconds takedTime;
        const auto TimerCallback = [&takedTime](std::chrono::microseconds scopedTime)
        {
            takedTime = scopedTime;
        };

        if constexpr (std::is_same_v<decltype(function()), void>)
        {
            {
                ScopedTimer timer{ TimerCallback };
                std::forward<FunctionT>(function)();
            }

            return takedTime;
        }
        else
        {
            const ScopedTimer timer{ TimerCallback };
            decltype(auto) functionResult = std::forward<FunctionT>(function)();
            timer.ForceDestroy();

            return std::make_pair(takedTime, std::move(functionResult));
        }
    }

} // namespace benzin

#define BenzinLogTimeOnScopeExit(...) const ::benzin::ScopedLogTimer BenzinUniqueVariableName(ScopedLogTimer){ __VA_ARGS__ }
#define BenzinProfileFunction(function) ProfileFunction([&]{ return function; })
