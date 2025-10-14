#pragma once

namespace benzin
{

    template <std::unsigned_integral T, std::unsigned_integral U>
    constexpr auto AlignUp(T value, U alignment)
    {
        using CommonType = std::common_type_t<T, U>;

        const CommonType commonValue = value;
        const CommonType commonAlignment = alignment;

        return (commonValue + (commonAlignment - 1)) & ~(commonAlignment - 1);
    }   

    template <std::unsigned_integral T>
    constexpr T DivideUp(T value, T divisor)
    {
        return (value + divisor - 1) / divisor;
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

    bool IsMatrixEqual(const DirectX::XMMATRIX lhs, const DirectX::XMMATRIX& rhs, float epsilon = 1e-6f);

}
