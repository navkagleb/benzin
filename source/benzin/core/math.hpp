#pragma once

namespace benzin
{

    template <std::unsigned_integral T, std::unsigned_integral U>
    constexpr auto AlignAbove(T value, U alignment)
    {
        using CommonType = std::common_type_t<T, U>;

        const CommonType commonValue = value;
        const CommonType commonAlignment = alignment;

        return (commonValue + (commonAlignment - 1)) & ~(commonAlignment - 1);
    }   

    template <std::unsigned_integral T>
    constexpr auto DivideUp(T value, T divisor)
    {
        return (value + divisor - 1) / divisor;
    }

    template <std::integral T>
    constexpr bool IsEvenQuickly(T value)
    {
        return (value & 1) == 0;
    }

    template <std::integral T>
    constexpr bool IsOddQuickly(T value)
    {
        return (value & 1) == 1;
    }

    template <std::integral T>
    constexpr bool IsDividedBy2Quickly(T value)
    {
        return (value & 2) == 0;
    }

    template <std::unsigned_integral T>
    constexpr T FindPowerOf2(T value)
    {
        T power = 0;

        while (value > 1)
        {
            value >>= 1;
            ++power;
        }

        return power;
    }
}
