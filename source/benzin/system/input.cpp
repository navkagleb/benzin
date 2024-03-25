#include "benzin/config/bootstrap.hpp"
#include "benzin/system/input.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    static bool g_IsAllKeyEventsBlocked = false;

    void Input::BlockKeyEvents()
    {
        g_IsAllKeyEventsBlocked = true;
    }

    void Input::UnblockKeyEvents()
    {
        g_IsAllKeyEventsBlocked = false;
    }

    bool Input::IsMouseButtonPressed(MouseButton mouseButton)
    {
        return ::GetAsyncKeyState((int)mouseButton) & 0x8000;
    }

    bool Input::IsKeyPressed(KeyCode keyCode)
    {
        if (g_IsAllKeyEventsBlocked)
        {
            return false;
        }

        return ::GetAsyncKeyState((int)keyCode) & 0x8000;
    }

    DirectX::XMINT2 Input::GetMousePosition(const Window& window)
    {
        BenzinAssert(window.GetWin64Window());

        POINT mousePosition{ 0, 0 };

        BenzinAssert(::GetCursorPos(&mousePosition) != 0);
        BenzinAssert(::ScreenToClient(window.GetWin64Window(), &mousePosition) != 0);

        return { mousePosition.x, mousePosition.y };
    }

} // namespace benzin
