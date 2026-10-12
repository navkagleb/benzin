#pragma once

namespace benzin
{

    template <std::unsigned_integral T, std::integral U>
    constexpr auto AlignUp(T value, U alignment)
    {
        return (value + ((T)alignment - 1)) & ~((T)alignment - 1);
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

    DirectX::XMVECTOR GetDirectionFromPitchYaw(float pitch, float yaw);
    DirectX::XMFLOAT2 GetPitchYawFromDirection(const DirectX::XMVECTOR& direction);

}
