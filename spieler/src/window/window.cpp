#include "window.h"

#include <imgui/backends/imgui_impl_win32.h>

#include "common.h"
#include "window_event.h"
#include "mouse_event.h"
#include "key_event.h"
#include "input.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace Spieler
{

    bool WindowRegisterClass::Init()
    {
        m_Info.cbSize          = sizeof(WNDCLASSEX);
        m_Info.style           = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
        m_Info.cbClsExtra      = 0;
        m_Info.cbWndExtra      = 0;
        m_Info.hInstance       = ::GetModuleHandle(nullptr);
        m_Info.hIcon           = ::LoadIcon(nullptr, IDI_APPLICATION);
        m_Info.hCursor         = ::LoadCursor(nullptr, IDC_ARROW);
        m_Info.hbrBackground   = nullptr;
        m_Info.lpszClassName   = m_Name.c_str();
        m_Info.hIconSm         = ::LoadIcon(nullptr, IDI_APPLICATION);
        m_Info.lpszMenuName    = nullptr;
        m_Info.lpfnWndProc     = [](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> LRESULT
        {
            Window* window = nullptr;

            if (msg == WM_NCCREATE)
            {
                window = static_cast<Window*>(reinterpret_cast<CREATESTRUCT*>(lparam)->lpCreateParams);

                {
                    const auto status = ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

                    if (!status)
                    {
                        if (::GetLastError() != 0)
                        {
                            return false;
                        }
                    }
                }
            }
            else
            {
                window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            }

#if SPIELER_USE_IMGUI
            if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
            {
                return true;
            }
#endif
            if (!window || !window->m_EventCallback)
            {
                return ::DefWindowProc(hwnd, msg, wparam, lparam);
            }

            static std::uint32_t width  = window->m_Width;
            static std::uint32_t height = window->m_Height;

            if (window && window->m_EventCallback)
            {
                switch (msg)
                {
                    // Window events
                    case WM_CLOSE:
                    {
                        WindowCloseEvent event;
                        window->m_EventCallback(event);

                        ::DestroyWindow(hwnd);

                        return 0;
                    }

                    case WM_ACTIVATE:
                    {
                        if (LOWORD(wparam) == WA_INACTIVE)
                        {
                            WindowUnfocusedEvent event;
                            window->m_EventCallback(event);
                        }
                        else
                        {
                            WindowFocusedEvent event;
                            window->m_EventCallback(event);
                        }

                        return 0;
                    }

                    case WM_SIZE:
                    {
                        width   = LOWORD(lparam);
                        height  = HIWORD(lparam);

                        switch (wparam)
                        {
                            case SIZE_MINIMIZED:
                            {
                                window->m_Width         = width;
                                window->m_Height        = height;
                                window->m_IsMinimized   = true;
                                window->m_IsMaximized   = false;

                                WindowMinimizedEvent event;
                                window->m_EventCallback(event);

                                break;
                            }

                            case SIZE_MAXIMIZED:
                            {
                                window->m_Width         = width;
                                window->m_Height        = height;
                                window->m_IsMinimized   = false;
                                window->m_IsMaximized   = true;

                                {
                                    WindowMaximizedEvent event;
                                    window->m_EventCallback(event);
                                }
                                
                                {
                                    WindowResizedEvent event(window->m_Width, window->m_Height);
                                    window->m_EventCallback(event);
                                }

                                break;
                            }

                            case SIZE_RESTORED:
                            {
                                if (window->m_IsResizing)
                                {
                                    WindowResizingEvent event(window->m_Width, window->m_Height);
                                    window->m_EventCallback(event);
                                }
                                else if (window->m_IsMinimized)
                                {
                                    window->m_Width         = width;
                                    window->m_Height        = height;
                                    window->m_IsMinimized   = false;

                                    {
                                        WindowRestoredEvent event;
                                        window->m_EventCallback(event);
                                    }
                                    
                                    {
                                        WindowResizedEvent event(window->m_Width, window->m_Height);
                                        window->m_EventCallback(event);
                                    }
                                }
                                else if (window->m_IsMaximized)
                                {
                                    window->m_Width         = width;
                                    window->m_Height        = height;
                                    window->m_IsMaximized   = false;

                                    {
                                        WindowRestoredEvent event;
                                        window->m_EventCallback(event);
                                    }

                                    {
                                        WindowResizedEvent event(window->m_Width, window->m_Height);
                                        window->m_EventCallback(event);
                                    }
                                }
                                else
                                {
                                    window->m_Width     = width;
                                    window->m_Height    = height;

                                    // API call such as ::SetWindowPos or IDXGISwapChain::SetFullscreenState
                                    {
                                        WindowRestoredEvent event;
                                        window->m_EventCallback(event);
                                    }

                                    {
                                        WindowResizedEvent event(window->m_Width, window->m_Height);
                                        window->m_EventCallback(event);
                                    }
                                }

                                break;
                            }

                            default:
                            {
                                break;
                            }
                        }

                        return 0;
                    }

                    case WM_ENTERSIZEMOVE:
                    {
                        window->m_IsResizing = true;

                        WindowEnterResizingEvent event;
                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_EXITSIZEMOVE:
                    {
                        window->m_IsResizing = false;

                        {
                            WindowExitResizingEvent event;
                            window->m_EventCallback(event);
                        }

                        if (window->m_Width != width || window->m_Height != height)
                        {
                            window->m_Width     = width;
                            window->m_Height    = height;

                            WindowResizedEvent event(window->m_Width, window->m_Height);
                            window->m_EventCallback(event);
                        }

                        return 0;
                    }

                    case WM_GETMINMAXINFO:
                    {
                        reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.x = 200;
                        reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.y = 200;

                        return 0;
                    }

                    // Mouse events
                    case WM_LBUTTONDOWN:
                    {
                        MouseButtonPressedEvent event(MouseButton_Left, LOWORD(lparam), HIWORD(lparam));
                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_RBUTTONDOWN:
                    {
                        MouseButtonPressedEvent event(MouseButton_Right, LOWORD(lparam), HIWORD(lparam));
                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_MBUTTONDOWN:
                    {
                        MouseButtonPressedEvent event(MouseButton_Middle, LOWORD(lparam), HIWORD(lparam));
                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_LBUTTONUP:
                    {
                        MouseButtonReleasedEvent event(MouseButton_Left, LOWORD(lparam), HIWORD(lparam));
                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_RBUTTONUP:
                    {
                        MouseButtonReleasedEvent event(MouseButton_Right, LOWORD(lparam), HIWORD(lparam));
                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_MBUTTONUP:
                    {
                        MouseButtonReleasedEvent event(MouseButton_Middle, LOWORD(lparam), HIWORD(lparam));
                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_MOUSEMOVE:
                    {
                        MouseMovedEvent event(LOWORD(lparam), HIWORD(lparam));
                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_MOUSEWHEEL:
                    {
                        MouseScrolledEvent event(GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1);
                        window->m_EventCallback(event);

                        return 0;
                    }

                    // Key event
                    case WM_KEYDOWN:
                    case WM_SYSKEYDOWN:
                    {
                        KeyPressedEvent event(static_cast<KeyCode>(wparam), static_cast<bool>(HIWORD(lparam) & KF_REPEAT));
                        window->m_EventCallback(event);

                        break;
                    }

                    case WM_KEYUP:
                    case WM_SYSKEYUP:
                    {
                        KeyReleasedEvent event(static_cast<KeyCode>(wparam));
                        window->m_EventCallback(event);

                        break;
                    }

                    case WM_CHAR:
                    case WM_SYSCHAR:
                    {
                        KeyTypedEvent event(wparam);
                        window->m_EventCallback(event);

                        break;
                    }

                    default:
                    {
                        break;
                    }
                }
            }

            return ::DefWindowProc(hwnd, msg, wparam, lparam);
        };

        const auto status = ::RegisterClassEx(&m_Info);

        if (!status)
        {
            return false;
        }

        m_IsInitialized = true;

        return true;
    }

    WindowRegisterClass::~WindowRegisterClass()
    {
        ::UnregisterClass(m_Name.c_str(), ::GetModuleHandle(nullptr));
    }

    WindowRegisterClass& Window::GetWindowRegisterClass()
    {
        static WindowRegisterClass instance;

        if (!instance.IsInitialized())
        {
            instance.Init();
        }

        return instance;
    }

    void Window::SetTitle(const std::string& title)
    {
        ::SetWindowText(m_Handle, title.c_str());
    }

    void Window::SetEventCallbackFunction(const EventCallbackFunction& callback)
    {
        m_EventCallback = callback;
    }

    bool Window::Init(const std::string& title, std::uint32_t width, std::uint32_t height)
    {
        m_Title     = title;
        m_Width     = width;
        m_Height    = height;

        RECT windowBounds = { 0, 0, static_cast<std::int32_t>(m_Width), static_cast<std::int32_t>(m_Height) };

        SPIELER_RETURN_IF_FAILED(::AdjustWindowRect(&windowBounds, m_Style, false));

        m_Handle = ::CreateWindow(
            GetWindowRegisterClass().GetName().c_str(),
            m_Title.c_str(),
            m_Style,
            (::GetSystemMetrics(SM_CXSCREEN) - windowBounds.right) / 2,
            (::GetSystemMetrics(SM_CYSCREEN) - windowBounds.bottom) / 2,
            windowBounds.right - windowBounds.left,
            windowBounds.bottom - windowBounds.top,
            nullptr,
            nullptr,
            ::GetModuleHandle(nullptr),
            reinterpret_cast<void*>(this)
        );

        if (m_Handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        SPIELER_RETURN_IF_FAILED(Input::GetInstance().Init(m_Handle));

        return Show();
    }

    bool Window::Show()
    {
        ::ShowWindow(m_Handle, SW_SHOWDEFAULT);
        SPIELER_RETURN_IF_FAILED(::UpdateWindow(m_Handle));

        return true;
    }

} // namespace Spieler