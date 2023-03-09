#pragma once

namespace benzin
{

    template <std::integral T>
    constexpr inline T Align(T value, T alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

} // namespace benzin
