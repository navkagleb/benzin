#pragma once

#include "benzin/core/logger.hpp"

namespace benzin
{

    template <typename EnumT> requires std::is_enum_v<EnumT>
    void PrintEnumFlags(EnumT flags)
    {
        const auto names = magic_enum::enum_names<EnumT>();
        const auto values = magic_enum::enum_values<EnumT>();

        const auto rawFlags = std::to_underlying(flags);
        BenzinTrace("{:08b}: {}", rawFlags, rawFlags);

        for (const auto i : std::views::iota(0u, magic_enum::enum_count<EnumT>()))
        {
            const auto rawFlag = std::to_underlying(values[i]);
            if ((rawFlags & rawFlag) == rawFlag && rawFlag != 0)
            {
                BenzinTrace("{:08b}: ({}) {}", rawFlag, names[i], rawFlag);
            }
        }
    };

} // namespace benzin
