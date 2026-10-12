#include "benzin/config/bootstrap.hpp"
#include "benzin/system/input.hpp"

#include "benzin/system/window.hpp"

namespace benzin
{

    static bool g_IsAllKeyEventsBlocked = false;

    static bool g_IsCursorLocked = false;
    static POINT g_LockedCursorPosition{};

    //

    void Input::SetAllKeyEventsBlocked(bool isBlocked)
    {
        g_IsAllKeyEventsBlocked = isBlocked;
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
        BenzinAssert(window.GetWin64Window() != nullptr);

        POINT mousePosition{ 0, 0 };
        BenzinAssertExpr(::GetCursorPos(&mousePosition) != 0);
        BenzinAssertExpr(::ScreenToClient(window.GetWin64Window(), &mousePosition) != 0);

        return { mousePosition.x, mousePosition.y };
    }

    DirectX::XMINT2 Input::LockCursor(const Window& window)
    {
        if (!g_IsCursorLocked)
        {
            g_IsCursorLocked = true;
            BenzinAssertExpr(::GetCursorPos(&g_LockedCursorPosition) != 0);
        }

        POINT clientLockedCursorPosition = g_LockedCursorPosition;
        BenzinAssertExpr(::ScreenToClient(window.GetWin64Window(), &clientLockedCursorPosition) != 0);

        return { clientLockedCursorPosition.x, clientLockedCursorPosition.y };
    }

    void Input::UnlockCursor()
    {
        g_IsCursorLocked = false;
    }

    void Input::SetCursorPositionIfNeeded()
    {
        if (g_IsCursorLocked)
        {
            ::SetCursorPos(g_LockedCursorPosition.x, g_LockedCursorPosition.y);
        }
    }

} // namespace benzin
