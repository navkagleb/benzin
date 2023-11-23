#pragma once

#include "benzin/system/mouse_button.hpp"
#include "benzin/system/key_code.hpp"

namespace benzin
{

    class Window;

    class Input
    {
    private:
        friend WindowsMessageHandlerDeclaration();

    public:
        template <typename T = int32_t>
        static T GetMouseX(const Window& window) { return static_cast<T>(GetMousePosition(window).x); }

        template <typename T = int32_t>
        static T GetMouseY(const Window& window) { return static_cast<T>(GetMousePosition(window).y); }

        static bool IsMouseButtonPressed(MouseButton mouseButton);
        static bool IsKeyPressed(KeyCode keyCode);

    private:
        static POINT GetMousePosition(const Window& window);

    private:
        static inline bool ms_IsKeyEventsBlocked = false;
    };

} // namespace benzin
