#pragma once

#include <cstdint>

namespace Spieler
{

    // Using Win32 virtual key codes
    enum MouseButton : std::int32_t
    {
        MouseButton_Unknown = -1,

        MouseButton_Left    = 0x01,
        MouseButton_Right   = 0x02,
        MouseButton_Middle  = 0x04
    };

} // namespace Spieler