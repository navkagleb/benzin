#pragma once

#include "mouse_button.h"
#include "key_code.h"

#include <windows.h>

namespace Spieler
{

    class Input
    {
    public:
        static Input& GetInstance();

    public:
        bool Init(HWND windowHandle);

        // Mouse
        float GetMouseX();
        float GetMouseY();

        bool IsMouseButtonPressed(MouseButton mouseButton);

        // Keyboard
        bool IsKeyPressed(KeyCode keyCode);

    private:
        bool m_IsInit{ false };
        HWND m_WindowHandle{ nullptr }; 
    };

} // namespace Spieler