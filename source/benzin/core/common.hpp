#pragma once

namespace benzin
{

    constexpr uint64_t KBToBytes(uint64_t kb)
    {
        return kb * 1024;
    }

    constexpr uint64_t MBToBytes(uint64_t mb)
    {
        return mb * 1024 * 1024;
    }

    constexpr uint64_t GBToBytes(uint64_t gb)
    {
        return gb * 1024 * 1024 * 1024;
    }

    constexpr uint64_t BytesToKB(uint64_t bytes)
    {
        return (bytes / 1024) + (bytes % 1024 != 0);
    }

    constexpr uint64_t BytesToMB(uint64_t bytes)
    {
        const uint64_t kb = BytesToKB(bytes);
        return (kb / 1024) + (kb % 1024 != 0);
    }

    constexpr uint64_t BytesToGB(uint64_t bytes)
    {
        const uint64_t mb = BytesToMB(bytes);
        return (mb / 1024) + (mb % 1024 != 0);
    }

    template <std::integral T, std::integral U>
    constexpr inline std::common_type_t<T, U> AlignAbove(T value, U alignment)
    {
        using CommonType = std::common_type_t<T, U>;

        const CommonType commonValue = value;
        const CommonType commonAlignment = alignment;

        return (commonValue + (commonAlignment - 1)) & ~(commonAlignment - 1);
    }

    template <std::integral T>
    constexpr inline T ToBit(T bitPosition)
    {
        return 1 << bitPosition;
    }

} // namespace benzin
