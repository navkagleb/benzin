#include "window/input.h"

#include "common.h"

namespace Spieler
{

    Input& Input::GetInstance()
    {
        static Input instance;
        return instance;
    }

    bool Input::Init(HWND windowHandle)
    {
        m_WindowHandle  = windowHandle;
        m_IsInit        = true;

        return true;
    }

    float Input::GetMouseX()
    {
        SPIELER_ASSERT(m_IsInit);
        SPIELER_ASSERT(m_WindowHandle);

        ::POINT mousePosition{ 0, 0 };

        SPIELER_ASSERT(::GetCursorPos(&mousePosition));
        SPIELER_ASSERT(::ScreenToClient(m_WindowHandle, &mousePosition));

        return static_cast<float>(mousePosition.x);
    }

    float Input::GetMouseY()
    {
        SPIELER_ASSERT(m_IsInit);
        SPIELER_ASSERT(m_WindowHandle);

        ::POINT mousePosition{ 0, 0 };

        SPIELER_ASSERT(::GetCursorPos(&mousePosition));
        SPIELER_ASSERT(::ScreenToClient(m_WindowHandle, &mousePosition));

        return static_cast<float>(mousePosition.y);
    }

    bool Input::IsMouseButtonPressed(MouseButton mouseButton)
    {
        SPIELER_ASSERT(m_IsInit);
        SPIELER_ASSERT(m_WindowHandle);

        return ::GetKeyState(static_cast<std::int32_t>(mouseButton)) & 0x8000;
    }

    bool Input::IsKeyPressed(KeyCode keyCode)
    {
        SPIELER_ASSERT(m_IsInit);
        SPIELER_ASSERT(m_WindowHandle);

        return ::GetKeyState(static_cast<std::int32_t>(keyCode)) & 0x8000;
    }

} // namespace Spieler