#include "spieler/config/bootstrap.hpp"

#include "win64_window.hpp"

#if defined(SPIELER_PLATFORM_WINDOWS)

#include <third_party/imgui/imgui_impl_win32.h>

#include "spieler/core/logger.hpp"
#include "spieler/core/assert.hpp"

#include "spieler/system/window_event.hpp"
#include "spieler/system/mouse_event.hpp"
#include "spieler/system/key_event.hpp"

#include "win64_input.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace spieler
{

    static Win64_RegisterClass g_Win64RegisterClass(Win64_Window::MessageHandler);

    Win64_RegisterClass& Win64_RegisterClass::GetInstance()
    {
        return g_Win64RegisterClass;
    }

    Win64_RegisterClass::Win64_RegisterClass(MessageHandler messageHandler)
    {
        m_Info.cbSize = sizeof(WNDCLASSEX);
        m_Info.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
        m_Info.cbClsExtra = 0;
        m_Info.cbWndExtra = 0;
        m_Info.hInstance = ::GetModuleHandle(nullptr);
        m_Info.hIcon = ::LoadIcon(nullptr, IDI_APPLICATION);
        m_Info.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        m_Info.hbrBackground = nullptr;
        m_Info.lpszClassName = m_Name.c_str();
        m_Info.hIconSm = ::LoadIcon(nullptr, IDI_APPLICATION);
        m_Info.lpszMenuName = nullptr;
        m_Info.lpfnWndProc = messageHandler;

        SPIELER_ASSERT(::RegisterClassEx(&m_Info) != 0);
    }

    Win64_RegisterClass::~Win64_RegisterClass()
    {
        ::UnregisterClass(m_Name.c_str(), ::GetModuleHandle(nullptr));
    }

    LRESULT Win64_Window::MessageHandler(HWND handle, UINT messageCode, WPARAM wparam, LPARAM lparam)
    {
        if (messageCode == WM_CREATE)
        {
            ::SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lparam)->lpCreateParams));

            return 0;
        }

        Win64_Window* window{ reinterpret_cast<Win64_Window*>(::GetWindowLongPtr(handle, GWLP_USERDATA)) };

        if (ImGui_ImplWin32_WndProcHandler(handle, messageCode, wparam, lparam))
        {
            return 0;
        }

        if (!window || !window->m_EventCallback)
        {
            return ::DefWindowProc(handle, messageCode, wparam, lparam);
        }

        static uint32_t width{ window->m_Parameters.Width };
        static uint32_t height{ window->m_Parameters.Height };

        switch (messageCode)
        {
            // Window events
            case WM_CLOSE:
            {
                WindowCloseEvent event;
                window->m_EventCallback(event);

                //::DestroyWindow(handle);

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
                width = LOWORD(lparam);
                height = HIWORD(lparam);

                switch (wparam)
                {
                    case SIZE_MINIMIZED:
                    {
                        window->m_Parameters.Width = width;
                        window->m_Parameters.Height = height;
                        window->m_Parameters.IsMinimized = true;
                        window->m_Parameters.IsMaximized = false;

                        WindowMinimizedEvent event;
                        window->m_EventCallback(event);

                        break;
                    }
                    case SIZE_MAXIMIZED:
                    {
                        window->m_Parameters.Width = width;
                        window->m_Parameters.Height = height;
                        window->m_Parameters.IsMinimized = false;
                        window->m_Parameters.IsMaximized = true;

                        {
                            WindowMaximizedEvent event;
                            window->m_EventCallback(event);
                        }

                        {
                            WindowResizedEvent event(window->m_Parameters.Width, window->m_Parameters.Height);
                            window->m_EventCallback(event);
                        }

                        break;
                    }
                    case SIZE_RESTORED:
                    {
                        if (window->m_Parameters.IsResizing)
                        {
                            WindowResizingEvent event(window->m_Parameters.Width, window->m_Parameters.Height);
                            window->m_EventCallback(event);
                        }
                        else if (window->m_Parameters.IsMinimized)
                        {
                            window->m_Parameters.Width = width;
                            window->m_Parameters.Height = height;
                            window->m_Parameters.IsMinimized = false;

                            {
                                WindowRestoredEvent event;
                                window->m_EventCallback(event);
                            }

                            {
                                WindowResizedEvent event(window->m_Parameters.Width, window->m_Parameters.Height);
                                window->m_EventCallback(event);
                            }
                        }
                        else if (window->m_Parameters.IsMaximized)
                        {
                            window->m_Parameters.Width = width;
                            window->m_Parameters.Height = height;
                            window->m_Parameters.IsMaximized = false;

                            {
                                WindowRestoredEvent event;
                                window->m_EventCallback(event);
                            }

                            {
                                WindowResizedEvent event(window->m_Parameters.Width, window->m_Parameters.Height);
                                window->m_EventCallback(event);
                            }
                        }
                        else
                        {
                            window->m_Parameters.Width = width;
                            window->m_Parameters.Height = height;

                            // API call such as ::SetWindowPos or IDXGISwapChain::SetFullscreenState
                            {
                                WindowRestoredEvent event;
                                window->m_EventCallback(event);
                            }

                            {
                                WindowResizedEvent event(window->m_Parameters.Width, window->m_Parameters.Height);
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
                window->m_Parameters.IsResizing = true;

                WindowEnterResizingEvent event;
                window->m_EventCallback(event);

                return 0;
            }
            case WM_EXITSIZEMOVE:
            {
                window->m_Parameters.IsResizing = false;

                {
                    WindowExitResizingEvent event;
                    window->m_EventCallback(event);
                }

                if (window->m_Parameters.Width != width || window->m_Parameters.Height != height)
                {
                    window->m_Parameters.Width = width;
                    window->m_Parameters.Height = height;

                    WindowResizedEvent event(window->m_Parameters.Width, window->m_Parameters.Height);
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
                MouseButtonPressedEvent event(MouseButton::Left, LOWORD(lparam), HIWORD(lparam));
                window->m_EventCallback(event);

                return 0;
            }
            case WM_RBUTTONDOWN:
            {
                MouseButtonPressedEvent event(MouseButton::Right, LOWORD(lparam), HIWORD(lparam));
                window->m_EventCallback(event);

                return 0;
            }
            case WM_MBUTTONDOWN:
            {
                MouseButtonPressedEvent event(MouseButton::Middle, LOWORD(lparam), HIWORD(lparam));
                window->m_EventCallback(event);

                return 0;
            }
            case WM_LBUTTONUP:
            {
                MouseButtonReleasedEvent event(MouseButton::Left, LOWORD(lparam), HIWORD(lparam));
                window->m_EventCallback(event);

                return 0;
            }
            case WM_RBUTTONUP:
            {
                MouseButtonReleasedEvent event(MouseButton::Right, LOWORD(lparam), HIWORD(lparam));
                window->m_EventCallback(event);

                return 0;
            }
            case WM_MBUTTONUP:
            {
                MouseButtonReleasedEvent event(MouseButton::Middle, LOWORD(lparam), HIWORD(lparam));
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
                KeyTypedEvent event(static_cast<char>(wparam));
                window->m_EventCallback(event);

                break;
            }
            default:
            {
                break;
            }
        }

        return ::DefWindowProc(handle, messageCode, wparam, lparam);
    }

    Win64_Window::Win64_Window(const std::string& title, uint32_t width, uint32_t height)
        : Window(title, width, height)
    {
        ::RECT windowBounds
        {
            .left = 0,
            .top = 0,
            .right = static_cast<LONG>(m_Parameters.Width),
            .bottom = static_cast<LONG>(m_Parameters.Height)
        };

        SPIELER_ASSERT(::AdjustWindowRect(&windowBounds, m_Style, false));

        m_NativeHandle = ::CreateWindow(
            Win64_RegisterClass::GetInstance().GetName().c_str(),
            m_Parameters.Title.c_str(),
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

        SPIELER_ASSERT(m_NativeHandle != nullptr);

        m_Input.SetInputPlatform(std::make_unique<Win64_Input>(GetNativeHandle<::HWND>()));

        SetVisible(true);
    }

    Win64_Window::~Win64_Window()
    {
        ::SetWindowLongPtr(GetNativeHandle<::HWND>(), GWLP_USERDATA, 0);
        ::DestroyWindow(GetNativeHandle<::HWND>());
    }

    // Window
    void Window::ProcessEvents()
    {
        ::MSG message{ nullptr };

        while (::PeekMessageW(&message, GetNativeHandle<::HWND>(), 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }

    void Window::SetTitle(const std::string& title)
    {
        ::SetWindowText(GetNativeHandle<::HWND>(), title.c_str());
    }
    
    void Window::SetVisible(bool isVisible)
    {
        ::ShowWindow(GetNativeHandle<::HWND>(), isVisible ? SW_SHOW : SW_HIDE);
        ::UpdateWindow(GetNativeHandle<::HWND>());
    }

} // namespace spieler

#endif // defined(SPIELER_WIN64)