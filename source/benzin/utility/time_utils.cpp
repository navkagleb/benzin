#include "benzin/config/bootstrap.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    namespace
    {

        template <typename DurationToT, typename RatioFromT = std::ratio<1>>
        auto FloatDurationTo(float duration)
        {
            return std::chrono::round<DurationToT>(std::chrono::duration<float, RatioFromT>{ duration });
        }

        template <typename DurationToT, typename DurationFromT>
        auto DurationTo(DurationFromT duration)
        {
            return std::chrono::duration_cast<DurationToT>(duration);
        }

    } // anonymous namespace

    std::chrono::microseconds SecToUs(float sec) { return FloatDurationTo<std::chrono::microseconds>(sec); }

    std::chrono::microseconds MsToUs(float ms) { return FloatDurationTo<std::chrono::microseconds, std::milli>(ms); }

    std::chrono::milliseconds SecToMs(float sec) { return FloatDurationTo<std::chrono::milliseconds>(sec); }

    std::chrono::milliseconds ToMs(std::chrono::microseconds us) { return DurationTo<std::chrono::milliseconds>(us); }

    std::chrono::microseconds ToUs(std::chrono::nanoseconds ns) { return DurationTo<std::chrono::microseconds>(ns); }

    float ToFloatMs(std::chrono::microseconds us) { return us.count() / 1000.0f; }

    float ToFloatSec(std::chrono::microseconds us) { return ToFloatMs(us) / 1000.0f; }

} // namespace benzin
