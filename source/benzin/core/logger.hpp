#pragma once

#include "benzin/core/enum_flags.hpp"

namespace benzin
{

    enum class LogOptionFlag
    {
        Time = ToBit(0),
        ThreadId = ToBit(1),
        FileName = ToBit(2),
        All = Time | ThreadId | FileName,
    };
    BenzinEnableFlagsForBitEnum(LogOptionFlag);

    namespace Logger
    {
        constexpr std::string_view GetLineSeparator()
        {
            return "-----------------------------------------------------------------------------------";
        }

        void Initialize(LogOptionFlags logOptionFlags = LogOptionFlag::All);
    };

}
