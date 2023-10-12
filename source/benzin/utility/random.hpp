#pragma once

namespace benzin
{

    class Random
    {
    public:
        BENZIN_NON_CONSTRUCTABLE_IMPL(Random)

    private:
        template <std::integral T>
        using IntegralDistribution = std::uniform_int_distribution<T>;

        template <std::floating_point T>
        using FloatingPointDistribution = std::uniform_real_distribution<T>;

    public:
        template <std::integral T>
        static T GetIntegral() { return GetIntegral(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()); }

        template <std::integral T>
        static T GetIntegral(T min, T max) { return IntegralDistribution<T>{ min, max }(ms_MersenneTwisterEngine); }

        template <std::floating_point T>
        static T GetFloatingPoint() { return GetFloatingPoint(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()); }

        template <std::floating_point T>
        static T GetFloatingPoint(T min, T max) { return FloatingPointDistribution<T>{ min, max }(ms_MersenneTwisterEngine); }

    private:
        static std::mt19937_64 ms_MersenneTwisterEngine;
    };

} // namespace benzin
