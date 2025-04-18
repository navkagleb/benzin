#include "benzin/config/bootstrap.hpp"
#include "benzin/system/window.hpp"

#include "benzin/core/profiler.hpp"
#include "benzin/graphics/common.hpp"
#include "benzin/system/input.hpp"
#include "benzin/system/key_event.hpp"
#include "benzin/system/mouse_event.hpp"
#include "benzin/system/window_event.hpp"

namespace benzin
{

    struct Win64_RegisterManager
    {
        static constexpr std::string_view s_Name = "BenzinWindowRegisterManager";

        Win64_RegisterManager()
        {
            const WNDCLASSEX registerClass
            {
                .cbSize = sizeof(WNDCLASSEX),
                .style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC,
                .lpfnWndProc = Window::MessageHandler,
                .cbClsExtra = 0,
                .cbWndExtra = 0,
                .hInstance = ::GetModuleHandle(nullptr),
                .hIcon = ::LoadIcon(nullptr, IDI_APPLICATION),
                .hCursor = ::LoadCursor(nullptr, IDC_ARROW),
                .hbrBackground = nullptr,
                .lpszMenuName = nullptr,
                .lpszClassName = s_Name.data(),
                .hIconSm = ::LoadIcon(nullptr, IDI_APPLICATION),
            };

            BenzinEnsure(::RegisterClassEx(&registerClass) != 0);
        }

        ~Win64_RegisterManager()
        {
            ::UnregisterClass(s_Name.data(), ::GetModuleHandle(nullptr));
        }
    };

    static Win64_RegisterManager g_RegisterManager;

    //

    Window::Window(const WindowCreation& creation)
        : m_Width{ creation.Width }
        , m_Height{ creation.Height }
    {
        auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (creation.IsResizable)
        {
            style |= WS_THICKFRAME;
            style |= WS_MAXIMIZEBOX;
        }

        RECT windowBounds
        {
            .left = 0,
            .top = 0,
            .right = (LONG)m_Width,
            .bottom = (LONG)m_Height,
        };

        BenzinEnsure(::AdjustWindowRect(&windowBounds, style, false) != 0);

        m_Win64Window = ::CreateWindow(
            Win64_RegisterManager::s_Name.data(),
            creation.Title.data(),
            style,
            (::GetSystemMetrics(SM_CXSCREEN) - windowBounds.right) / 2,
            (::GetSystemMetrics(SM_CYSCREEN) - windowBounds.bottom) / 2,
            windowBounds.right - windowBounds.left,
            windowBounds.bottom - windowBounds.top,
            nullptr,
            nullptr,
            ::GetModuleHandle(nullptr),
            (void*)this
        );

        BenzinEnsure(m_Win64Window != nullptr);

        BenzinTrace("Window is created: {} x {}", m_Width, m_Height);
    }

    Window::~Window()
    {
        // Reset callbacks before executing 'MessageHandler' to avoid handling events after it is destroyed
        m_PreMessageHandlerCallback = nullptr;
        m_EventCallback = nullptr;

        BenzinEnsure(m_Win64Window != nullptr);

        ::DestroyWindow(m_Win64Window);
    }

    void Window::ProcessEvents()
    {
        BenzinProfile();

        MSG message{ nullptr };

        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }

    void Window::SetTitle(std::string_view title)
    {
        ::SetWindowText(m_Win64Window, title.data());
    }

    void Window::SetVisible(bool isVisible)
    {
        // #TODO: Add assertions

        ::ShowWindow(m_Win64Window, isVisible ? SW_SHOW : SW_HIDE);
        ::UpdateWindow(m_Win64Window);
    }

    LRESULT Window::MessageHandler(HWND windowHandle, UINT messageCode, WPARAM wparam, LPARAM lparam)
    {
        if (messageCode == WM_CREATE)
        {
            ::SetWindowLongPtr(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lparam)->lpCreateParams));
            return 0;
        }
        else if (messageCode == WM_DESTROY)
        {
            ::SetWindowLongPtr(windowHandle, GWLP_USERDATA, 0);
        }

        auto* window = reinterpret_cast<Window*>(::GetWindowLongPtr(windowHandle, GWLP_USERDATA));

        if (window != nullptr && window->m_PreMessageHandlerCallback)
        {
            if (window->m_PreMessageHandlerCallback(windowHandle, messageCode, wparam, lparam))
            {
                return true;
            }
        }

        if (window != nullptr && window->m_EventCallback)
        {
            bool isEventHandled = false;
            isEventHandled |= HandleWindowEvents(*window, messageCode, wparam, lparam);
            isEventHandled |= HandleMouseEvents(*window, messageCode, wparam, lparam);
            isEventHandled |= HandleKeyEvents(*window, messageCode, wparam, lparam);

            if (isEventHandled)
            {
                return 0;
            }
        }

        return ::DefWindowProc(windowHandle, messageCode, wparam, lparam);
    }

    bool Window::HandleWindowEvents(Window& window, UINT messageCode, WPARAM wparam, LPARAM lparam)
    {
        static uint32_t width = window.m_Width;
        static uint32_t height = window.m_Height;

        switch (messageCode)
        {
        case WM_CLOSE:
        {
            window.CreateAndPushEvent<WindowCloseEvent>();
            return true;
        }
        case WM_ACTIVATE:
        {
            if (LOWORD(wparam) == WA_INACTIVE)
            {
                Input::SetAllKeyEventsBlocked(true);

                window.m_IsFocused = false;
                window.CreateAndPushEvent<WindowUnfocusedEvent>();
            }
            else
            {
                Input::SetAllKeyEventsBlocked(false);

                window.m_IsFocused = true;
                window.CreateAndPushEvent<WindowFocusedEvent>();
            }

            return true;
        }
        case WM_SIZE:
        {
            width = LOWORD(lparam);
            height = HIWORD(lparam);

            switch (wparam)
            {
            case SIZE_MINIMIZED:
            {
                window.m_Width = width;
                window.m_Height = height;
                window.m_IsMinimized = true;
                window.m_IsMaximized = false;

                window.CreateAndPushEvent<WindowMinimizedEvent>();

                return true;
            }
            case SIZE_MAXIMIZED:
            {
                window.m_Width = width;
                window.m_Height = height;
                window.m_IsMinimized = false;
                window.m_IsMaximized = true;

                window.CreateAndPushEvent<WindowMaximizedEvent>();
                window.CreateAndPushEvent<WindowResizedEvent>(window.m_Width, window.m_Height);

                return true;
            }
            case SIZE_RESTORED:
            {
                if (window.m_IsResizing)
                {
                    window.CreateAndPushEvent<WindowResizingEvent>(window.m_Width, window.m_Height);
                }
                else
                {
                    if (window.m_IsMinimized)
                    {
                        window.m_IsMinimized = false;
                    }
                    else if (window.m_IsMaximized)
                    {
                        window.m_IsMaximized = false;
                    }

                    window.m_Width = width;
                    window.m_Height = height;

                    window.CreateAndPushEvent<WindowRestoredEvent>();
                    window.CreateAndPushEvent<WindowResizedEvent>(window.m_Width, window.m_Height);
                }

                return true;
            }
            }
            return false;
        }
        case WM_ENTERSIZEMOVE:
        {
            window.m_IsResizing = true;
            window.CreateAndPushEvent<WindowEnterResizingEvent>();

            return true;
        }
        case WM_EXITSIZEMOVE:
        {
            window.m_IsResizing = false;

            window.CreateAndPushEvent<WindowExitResizingEvent>();

            if (window.m_Width != width || window.m_Height != height)
            {
                window.m_Width = width;
                window.m_Height = height;

                window.CreateAndPushEvent<WindowResizedEvent>(window.m_Width, window.m_Height);
            }

            return true;
        }
        case WM_GETMINMAXINFO:
        {
            reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.x = 200;
            reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.y = 200;

            return true;
        }
        }

        return false;
    }

    bool Window::HandleMouseEvents(Window& window, UINT messageCode, WPARAM wparam, LPARAM lparam)
    {
        switch (messageCode)
        {
        case WM_LBUTTONDOWN:
        {
            window.CreateAndPushEvent<MouseButtonPressedEvent>(MouseButton::Left, LOWORD(lparam), HIWORD(lparam));
            return true;
        }
        case WM_RBUTTONDOWN:
        {
            window.CreateAndPushEvent<MouseButtonPressedEvent>(MouseButton::Right, LOWORD(lparam), HIWORD(lparam));
            return true;
        }
        case WM_MBUTTONDOWN:
        {
            window.CreateAndPushEvent<MouseButtonPressedEvent>(MouseButton::Middle, LOWORD(lparam), HIWORD(lparam));
            return true;
        }
        case WM_LBUTTONUP:
        {
            window.CreateAndPushEvent<MouseButtonReleasedEvent>(MouseButton::Left, LOWORD(lparam), HIWORD(lparam));
            return true;
        }
        case WM_RBUTTONUP:
        {
            window.CreateAndPushEvent<MouseButtonReleasedEvent>(MouseButton::Right, LOWORD(lparam), HIWORD(lparam));
            return true;
        }
        case WM_MBUTTONUP:
        {
            window.CreateAndPushEvent<MouseButtonReleasedEvent>(MouseButton::Middle, LOWORD(lparam), HIWORD(lparam));
            return true;
        }
        case WM_MOUSEMOVE:
        {
            window.CreateAndPushEvent<MouseMovedEvent>(LOWORD(lparam), HIWORD(lparam));
            return true;
        }
        case WM_MOUSEWHEEL:
        {
            window.CreateAndPushEvent<MouseScrolledEvent>((int8_t)(GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1));
            return true;
        }
        }

        return false;
    }

    bool Window::HandleKeyEvents(Window& window, UINT messageCode, WPARAM wparam, LPARAM lparam)
    {
        switch (messageCode)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            window.CreateAndPushEvent<KeyPressedEvent>((KeyCode)wparam, (bool)(HIWORD(lparam) & KF_REPEAT));
            return true;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            window.CreateAndPushEvent<KeyReleasedEvent>((KeyCode)wparam);
            return true;
        }
        case WM_CHAR:
        case WM_SYSCHAR:
        {
            window.CreateAndPushEvent<KeyTypedEvent>((char)wparam);
            return true;
        }
        }

        return false;
    }

}
