#pragma once

namespace benzin
{

    constexpr uint64_t ConvertKBToBytes(uint64_t kb)
    {
        return kb * 1024;
    }

    constexpr uint64_t ConvertMBToBytes(uint64_t mb)
    {
        return mb * 1024 * 1024;
    }

    constexpr uint64_t ConvertGBToBytes(uint64_t gb)
    {
        return gb * 1024 * 1024 * 1024;
    }

    constexpr uint64_t ConvertBytesToKB(uint64_t bytes)
    {
        return (bytes / 1024) + (bytes % 1024 != 0);
    }

    constexpr uint64_t ConvertBytesToMB(uint64_t bytes)
    {
        const uint64_t kb = ConvertBytesToKB(bytes);
        return (kb / 1024) + (kb % 1024 != 0);
    }

    constexpr uint64_t ConvertBytesToGB(uint64_t bytes)
    {
        const uint64_t mb = ConvertBytesToMB(bytes);
        return (mb / 1024) + (mb % 1024 != 0);
    }

    template <std::integral T>
    constexpr inline T AlignAbove(T value, T alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

} // namespace benzin
