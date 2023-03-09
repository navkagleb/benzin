#pragma once

namespace benzin
{

    // Using Win32 virtual key codes
    enum class MouseButton : int8_t
    {
        Unknown = -1,

        Left = 0x01,
        Right = 0x02,
        Middle = 0x04
    };

} // namespace benzin