#pragma once

namespace benzin
{

    class TickTimer
    {
    public:
        auto GetDeltaTime() const { return m_DeltaTime; }
        auto GetElapsedTimeInSec() const { return m_ElapsedTimeInSec; }

        auto GetDeltaTimeInMs() const { return m_DeltaTime.count() / 1000.0f / 1000.0f; }
        auto GetDeltaTimeInSec() const { return m_DeltaTime.count() / 1000.0f / 1000.0f / 1000.0f; }

        auto IsPaused() const { return m_IsPaused; }
        void SetPaused(bool isPaused);

        void Reset();
        void Tick();

    private:
        std::chrono::high_resolution_clock::time_point m_CurrentTimePoint = {};
        std::chrono::high_resolution_clock::time_point m_PreviousTimePoint = {};

        std::chrono::high_resolution_clock::duration m_DeltaTime = {};
        float m_ElapsedTimeInSec = 0.0f;

        bool m_IsPaused = false;
    };

    class IntervalTimer
    {
    public:
        using Callback = std::function<void(float timeInMs, uint32_t frameCount)>;

        IntervalTimer(const TickTimer& baseTimer, std::chrono::microseconds interval);

        void AddCallback(Callback&& callback) const;
        void AccumulateInterval();

    private:
        const TickTimer& m_BaseTimer;
        const std::chrono::microseconds m_Interval = {};

        std::chrono::microseconds m_AccumulatedInterval = {};
        uint32_t m_AccumulatedFrameCount = 0;

        mutable std::vector<Callback> m_Callbacks;
    };

    class ScopedLogTimer
    {
    public:
        template <typename... Args>
        ScopedLogTimer(std::format_string<Args...> fmt, Args&&... args)
            : m_ScopeName{ std::format(fmt, std::forward<Args>(args)...) }
            , m_BeginTimePoint{ std::chrono::high_resolution_clock::now() }
        {}

        ~ScopedLogTimer()
        {
            const auto takenTime = std::chrono::high_resolution_clock::now() - m_BeginTimePoint;
            BenzinTrace("{} ({:.3f} ms)", m_ScopeName, takenTime.count() / 1000.0f / 1000.0f);
        }

    private:
        std::string m_ScopeName;
        std::chrono::high_resolution_clock::time_point m_BeginTimePoint;
    };

    template <typename FunctionT>
    auto ProfileFunction(FunctionT&& function)
    {
        using std::chrono::high_resolution_clock;
        using std::chrono::duration_cast;

        if constexpr (std::is_void_v<decltype(function())>)
        {
            const auto beginTimePoint = high_resolution_clock::now();
            function();
            return high_resolution_clock::now() - beginTimePoint;
        }
        else
        {
            const auto beginTimePoint = high_resolution_clock::now();
            decltype(auto) functionResult = function();
            const auto takenTime = high_resolution_clock::now() - beginTimePoint;

            return std::make_pair(takenTime, std::move(functionResult));
        }
    }

}

#define BenzinTraceScopeTime(...) const ::benzin::ScopedLogTimer BenzinUniqueVariableName(ScopedLogTimer){ __VA_ARGS__ }
#define BenzinProfileFunction(function) ::benzin::ProfileFunction([&]{ return function; })