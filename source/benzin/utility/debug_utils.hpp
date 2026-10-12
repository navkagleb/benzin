#pragma once

namespace benzin
{

    template <EnumConcept EnumT>
    void PrintEnumNamesAndValues()
    {
        const auto names = magic_enum::enum_names<EnumT>();
        const auto values = magic_enum::enum_values<EnumT>();

        std::string buffer;
        for (const auto& [name, value] : std::views::zip(names, values))
        {
            const auto integerValue = magic_enum::enum_integer(value);
            std::format_to(std::back_inserter(buffer), "{:3} | 0b{:8b} | 0x{:2x} | {}\n", integerValue, integerValue, integerValue, name);
        }

        BenzinTrace("\n{}", buffer);
    }

    template <EnumConcept EnumT>
    void PrintEnumFlags(EnumT flags)
    {
        const auto names = magic_enum::enum_names<EnumT>();
        const auto values = magic_enum::enum_values<EnumT>();

        const auto rawFlags = magic_enum::enum_integer(flags);
        BenzinTrace("Flags: {:3} | 0b{:8b} | 0x{:2x}", rawFlags, rawFlags, rawFlags);

        for (const auto [name, value] : std::views::zip(names, values))
        {
            const auto rawFlag = magic_enum::enum_integer(value);
            if ((rawFlags & rawFlag) == rawFlag && rawFlag != 0)
            {
                BenzinTrace("{:3} | 0b{:8b} | 0x{:2x} | {}", rawFlag, rawFlag, rawFlag, name);
            }
        }
    };

} // namespace benzin
