#include "benzin/config/bootstrap.hpp"
#include "benzin/system/input.hpp"

#include "benzin/system/window.hpp"

namespace benzin
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
        BenzinAssert(window.GetWin64Window());

        POINT mousePosition{ 0, 0 };

        BenzinAssert(::GetCursorPos(&mousePosition) != 0);
        BenzinAssert(::ScreenToClient(window.GetWin64Window(), &mousePosition) != 0);

        return mousePosition;
    }

} // namespace benzin
