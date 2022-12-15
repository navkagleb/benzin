#pragma once

namespace spieler
{

    class Random
    {
    private:
        SPIELER_NON_COPYABLE(Random);
        SPIELER_NON_MOVEABLE(Random);

    private:
        template <std::integral T>
        using IntegralDistribution = std::uniform_int_distribution<T>;

        template <std::floating_point T>
        using FloatingPointDistribution = std::uniform_real_distribution<T>;

    public:
        static Random& GetInstance();

    public:
        // TODO: move to private section
        Random();

    public:
        template <std::integral T>
        T GetIntegral();

        template <std::integral T>
        T GetIntegral(T min, T max);

        template <std::floating_point T>
        T GetFloatingPoint();

        template <std::floating_point T>
        T GetFloatingPoint(T min, T max);

    private:
        std::mt19937_64 m_MersenneTwisterEngine;
    };

    template <std::integral T>
    T Random::GetIntegral()
    {
        return GetIntegral(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    }

    template <std::integral T>
    T Random::GetIntegral(T min, T max)
    {
        return IntegralDistribution<T>{ min, max }(m_MersenneTwisterEngine);
    }

    template <std::floating_point T>
    T Random::GetFloatingPoint()
    {
        return GetFloatingPoint(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    }

    template <std::floating_point T>
    T Random::GetFloatingPoint(T min, T max)
    {
        return FloatingPointDistribution<T>{ min, max }(m_MersenneTwisterEngine);
    }

} // namespace spieler