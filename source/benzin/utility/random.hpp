#pragma once

namespace benzin
{

    class Random
    {
    public:
        BenzinDefineNonConstructable(Random);

    public:
        template <std::integral T>
        static T GetNumber() { return GetNumber(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()); }

        template <std::integral T>
        static T GetNumber(T min, T max) { return std::uniform_int_distribution<T>{ min, max }(ms_MersenneTwisterEngine); }

        template <std::floating_point T>
        static T GetNumber() { return GetNumber(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()); }

        template <std::floating_point T>
        static T GetNumber(T min, T max) { return std::uniform_real_distribution<T>{ min, max }(ms_MersenneTwisterEngine); }

    private:
        static std::mt19937_64 GetInitializedRandomEngine()
        {
            const std::seed_seq seed
            {
                std::random_device{}(),
                static_cast<std::seed_seq::result_type>(std::chrono::steady_clock::now().time_since_epoch().count())
            };

            return std::mt19937_64{ seed };
        }

    private:
        static inline thread_local std::mt19937_64 ms_MersenneTwisterEngine{ GetInitializedRandomEngine() };
    };

} // namespace benzin
