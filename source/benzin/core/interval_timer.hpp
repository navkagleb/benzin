#pragma once

namespace benzin
{

    class TickTimer;

    class IntervalTimer
    {
    public:
        using Callback = std::function<void(float timeInMs, uint32_t frameCount)>;

        IntervalTimer(const TickTimer& baseTimer, std::chrono::microseconds interval);

        auto GetInterval() const { return m_Interval; }
        auto GetAccumulatedInterval() const { return m_AccumulatedInterval; }

        void AccumulateInterval();
        void AddCallback(Callback&& callback) const;

    private:
        const TickTimer& m_BaseTimer;

        const std::chrono::microseconds m_Interval{};

        std::chrono::microseconds m_AccumulatedInterval{};
        uint32_t m_AccumulatedFrameCount = 0;

        mutable std::vector<Callback> m_Callbacks;
    };

}
