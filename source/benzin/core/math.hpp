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

    constexpr uint32_t I32Ceil(float floatingPoint)
    {
        const auto integral = static_cast<int32_t>(floatingPoint);
        return floatingPoint > integral ? integral + 1 : integral;
    }

    constexpr uint32_t I32Floor(float floatingPoint)
    {
        const auto integral = static_cast<int32_t>(floatingPoint);
        return floatingPoint < integral ? integral - 1 : integral;
    }

    constexpr uint32_t GetDispatchGroupCount(uint32_t dimension, uint32_t groupSize)
    {
        const auto groupCount = I32Ceil((float)dimension / groupSize);
        return std::max<uint32_t>(groupCount, 1);
    }

}
