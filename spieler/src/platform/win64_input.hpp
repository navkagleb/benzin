#pragma once

#if defined(SPIELER_WIN64)

#include "spieler/system/input.hpp"

namespace spieler
{

    class Win64_Input : public InputPlatform
    {
    public:
        Win64_Input(::HWND windowHandle);
        ~Win64_Input() override = default;

    public:
        float GetMouseX() const;
        float GetMouseY() const;

        bool IsMouseButtonPressed(MouseButton mouseButton) const;
        bool IsKeyPressed(KeyCode keyCode) const;

    private:
        ::POINT GetMousePosition() const;

    private:
        ::HWND m_WindowHandle{ nullptr };
    };

} // namespace spieler

#endif // defined(SPIELER_WIN64)