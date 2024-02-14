#include "benzin/config/bootstrap.hpp"
#include "benzin/system/input.hpp"

#include "benzin/system/window.hpp"

namespace benzin
{

    static bool g_AreKeyEventsBlocked = false;

    void Input::SetAreKeyEventsBlocked(bool areKeyEventsBlocked)
    {
        g_AreKeyEventsBlocked = areKeyEventsBlocked;
    }

    bool Input::IsMouseButtonPressed(MouseButton mouseButton)
    {
        return ::GetAsyncKeyState((int)mouseButton) & 0x8000;
    }

    bool Input::IsKeyPressed(KeyCode keyCode)
    {
        if (g_AreKeyEventsBlocked)
        {
            return false;
        }

        return ::GetAsyncKeyState((int)keyCode) & 0x8000;
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
