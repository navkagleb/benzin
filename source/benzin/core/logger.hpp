#pragma once

#include "benzin/core/enum_flags.hpp"

namespace benzin
{

    enum class LogOptionFlag : uint32_t
    {
        Time = ToBit(0),
        ThreadId = ToBit(1),
        FileName = ToBit(2),
        All = Time | ThreadId | FileName,
    };
    BenzinEnableFlagsForBitEnum(LogOptionFlag);

    namespace Logger
    {
        void Initialize(LogOptionFlags logOptionFlags = LogOptionFlag::All);

        constexpr std::string_view GetLineSeparator()
        {
            return "-----------------------------------------------------------------------------------";
        }

        const std::locale& GetThoudandSeperatorApostrophe3();
    };

}
