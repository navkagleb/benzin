#include "window.h"

#include "common.h"
#include "window_event.h"

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

            if (window && window->m_EventCallback)
            {
                switch (msg)
                {
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
                        window->m_Width     = LOWORD(lparam);
                        window->m_Height    = HIWORD(lparam);

                        switch (wparam)
                        {
                            case SIZE_MINIMIZED:
                            {
                                WindowMinimizedEvent event;

                                window->m_EventCallback(event);

                                break;
                            }

                            case SIZE_MAXIMIZED:
                            {
                                WindowMaximizedEvent event;

                                window->m_EventCallback(event);

                                break;
                            }

                            case SIZE_RESTORED:
                            {
                                WindowRestoredEvent event;

                                window->m_EventCallback(event);

                                break;
                            }

                            default:
                            {
                                break;
                            }
                        }

                       

                        //WindowExitEvent event(window->m_Width, window->m_Height);

                        //window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_ENTERSIZEMOVE:
                    {
                        WindowEnterResizingEvent event;

                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_EXITSIZEMOVE:
                    {
                        WindowExitResizingEvent event(window->m_Width, window->m_Height);

                        window->m_EventCallback(event);

                        return 0;
                    }

                    case WM_GETMINMAXINFO:
                    {
                        reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.x = 200;
                        reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.y = 200;

                        return 0;
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

        SPIELER_CHECK_STATUS(::AdjustWindowRect(&windowBounds, m_Style, false));

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

        return true;
    }

    bool Window::Show()
    {
        ::ShowWindow(m_Handle, SW_SHOWDEFAULT);
        SPIELER_CHECK_STATUS(::UpdateWindow(m_Handle));

        return true;
    }

} // namespace Spieler