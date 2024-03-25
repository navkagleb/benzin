#pragma once

#include "benzin/system/mouse_button.hpp"
#include "benzin/system/key_code.hpp"

namespace benzin
{

    class Window;

    class Input
    {
    public:
        static void BlockKeyEvents();
        static void UnblockKeyEvents();

        template <typename T = int32_t>
        static T GetMouseX(const Window& window) { return (T)GetMousePosition(window).x; }

        template <typename T = int32_t>
        static T GetMouseY(const Window& window) { return (T)GetMousePosition(window).y; }

        static bool IsMouseButtonPressed(MouseButton mouseButton);
        static bool IsKeyPressed(KeyCode keyCode);

    private:
        DirectX::XMINT2 GetMousePosition(const Window& window);
    };

} // namespace benzin
