#pragma once

namespace benzin
{

    class TickTimer;

    class IntervalTimer
    {
    public:
        using Callback = std::function<void(uint32_t frameCount)>;

        explicit IntervalTimer(std::chrono::microseconds interval);

        auto GetInterval() const { return m_Interval; }
        auto GetAccumulatedInterval() const { return m_AccumulatedInterval; }

        void AccumulateInterval(const TickTimer& frameTimer);
        void PushCallback(Callback&& callback);

    private:
        const std::chrono::microseconds m_Interval{};

        std::chrono::microseconds m_AccumulatedInterval{};
        uint32_t m_AccumulatedFrameCount = 0;

        std::vector<Callback> m_Callbacks;
    };

}
