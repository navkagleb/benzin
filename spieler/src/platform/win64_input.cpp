#include "spieler/config/bootstrap.hpp"

#include "platform/win64_input.hpp"

#if defined(SPIELER_PLATFORM_WINDOWS)

#include "spieler/core/assert.hpp"

namespace spieler
{

    Win64_Input::Win64_Input(::HWND windowHandle)
        : m_WindowHandle(windowHandle)
    {}

    float Win64_Input::GetMouseX() const
    {
        return static_cast<float>(GetMousePosition().x);
    }

    float Win64_Input::GetMouseY() const
    {
        return static_cast<float>(GetMousePosition().y);
    }

    bool Win64_Input::IsMouseButtonPressed(MouseButton mouseButton) const
    {
        SPIELER_ASSERT(m_WindowHandle);

        return ::GetAsyncKeyState(static_cast<std::int32_t>(mouseButton)) & 0x8000;
    }

    bool Win64_Input::IsKeyPressed(KeyCode keyCode) const
    {
        SPIELER_ASSERT(m_WindowHandle);

        return ::GetAsyncKeyState(static_cast<std::int32_t>(keyCode)) & 0x8000;
    }

    ::POINT Win64_Input::GetMousePosition() const
    {
        SPIELER_ASSERT(m_WindowHandle);

        ::POINT mousePosition{ 0, 0 };

        SPIELER_ASSERT(::GetCursorPos(&mousePosition));
        SPIELER_ASSERT(::ScreenToClient(m_WindowHandle, &mousePosition));

        return mousePosition;
    }

} // namespace spieler

#endif // defined(SPILER_WIN64)