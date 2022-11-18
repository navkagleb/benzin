#pragma once

#include "spieler/system/mouse_button.hpp"
#include "spieler/system/key_code.hpp"

namespace spieler
{

    class Window;

    class Input
    {
    public:
        template <typename T>
        static T GetMouseX(const Window& window) { return static_cast<T>(GetMousePosition(window).x); }

        template <typename T>
        static T GetMouseY(const Window& window) { return static_cast<T>(GetMousePosition(window).y); }

        static bool IsMouseButtonPressed(MouseButton mouseButton);
        static bool IsKeyPressed(KeyCode keyCode);

    private:
        static POINT GetMousePosition(const Window& window);
    };

} // namespace spieler
