#include "spieler/config/bootstrap.hpp"

#include "spieler/system/input.hpp"

#include "spieler/system/window.hpp"

namespace spieler
{

    bool Input::IsMouseButtonPressed(MouseButton mouseButton)
    {
        return ::GetAsyncKeyState(static_cast<int>(mouseButton)) & 0x8000;
    }

    bool Input::IsKeyPressed(KeyCode keyCode)
    {
        return ::GetAsyncKeyState(static_cast<int>(keyCode)) & 0x8000;
    }

    POINT Input::GetMousePosition(const Window& window)
    {
        SPIELER_ASSERT(window.GetWin64Window());

        POINT mousePosition{ 0, 0 };

        SPIELER_ASSERT(::GetCursorPos(&mousePosition));
        SPIELER_ASSERT(::ScreenToClient(window.GetWin64Window(), &mousePosition));

        return mousePosition;
    }

} // namespace spieler
