#include "benzin/config/bootstrap.hpp"

#include "benzin/system/window.hpp"

#include <third_party/imgui/backends/imgui_impl_win32.h>

#include "benzin/system/window_event.hpp"
#include "benzin/system/mouse_event.hpp"
#include "benzin/system/key_event.hpp"

#include "benzin/graphics/graphics_command_list.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace benzin
{

    LRESULT MessageHandler(HWND handle, UINT messageCode, WPARAM wparam, LPARAM lparam)
    {
        if (messageCode == WM_CREATE)
        {
            ::SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lparam)->lpCreateParams));

            return 0;
        }

        Window* window{ reinterpret_cast<Window*>(::GetWindowLongPtr(handle, GWLP_USERDATA)) };

        if (ImGui_ImplWin32_WndProcHandler(handle, messageCode, wparam, lparam))
        {
            return 0;
        }

        if (!window || !window->m_EventCallback)
        {
            return ::DefWindowProc(handle, messageCode, wparam, lparam);
        }

        static uint32_t width{ window->m_Width };
        static uint32_t height{ window->m_Height };

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
                        window->m_Width = width;
                        window->m_Height = height;
                        window->m_IsMinimized = true;
                        window->m_IsMaximized = false;

                        WindowMinimizedEvent event;
                        window->m_EventCallback(event);

                        break;
                    }
                    case SIZE_MAXIMIZED:
                    {
                        window->m_Width = width;
                        window->m_Height = height;
                        window->m_IsMinimized = false;
                        window->m_IsMaximized = true;

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
                            window->m_Width = width;
                            window->m_Height = height;
                            window->m_IsMinimized = false;

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
                            window->m_Width = width;
                            window->m_Height = height;
                            window->m_IsMaximized = false;

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
                            window->m_Width = width;
                            window->m_Height = height;

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
                    window->m_Width = width;
                    window->m_Height = height;

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
    
    class RegisterManager
    {
    public:
        typedef LRESULT(*MessageHandler)(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    public:
        RegisterManager(const std::string_view& name, MessageHandler messageHandler)
            : m_Name{ name }
        {
            const WNDCLASSEX registerClass
            {
                .cbSize{ sizeof(WNDCLASSEX) },
                .style{ CS_VREDRAW | CS_HREDRAW | CS_OWNDC },
                .lpfnWndProc{ messageHandler },
                .cbClsExtra{ 0 },
                .cbWndExtra{ 0 },
                .hInstance{ ::GetModuleHandle(nullptr) },
                .hIcon{ ::LoadIcon(nullptr, IDI_APPLICATION) },
                .hCursor{ ::LoadCursor(nullptr, IDC_ARROW) },
                .hbrBackground{ nullptr },
                .lpszMenuName{ nullptr },
                .lpszClassName{ m_Name.data() },
                .hIconSm{ ::LoadIcon(nullptr, IDI_APPLICATION) },
            };

            BENZIN_ASSERT(::RegisterClassEx(&registerClass) != 0);
        }

        ~RegisterManager()
        {
            ::UnregisterClass(m_Name.data(), ::GetModuleHandle(nullptr));
        }

    public:
        const std::string_view& GetName() const { return m_Name; }

    private:
        const std::string_view m_Name;
    };

    static RegisterManager g_RegisterManager{ "SpielerRegisterClass", MessageHandler };

    Window::Window(const std::string& title, uint32_t width, uint32_t height)
        : m_Width{ width }
        , m_Height{ height }
    {
        const LONG m_Style{ WS_OVERLAPPEDWINDOW };

        RECT windowBounds
        {
            .left{ 0 },
            .top{ 0 },
            .right{ static_cast<LONG>(m_Width) },
            .bottom{ static_cast<LONG>(m_Height) }
        };

        BENZIN_ASSERT(::AdjustWindowRect(&windowBounds, m_Style, false));

        m_Win64Window = ::CreateWindow(
            g_RegisterManager.GetName().data(),
            title.c_str(),
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

        BENZIN_ASSERT(m_Win64Window != nullptr);

        SetVisible(true);

        BENZIN_INFO("Window created");
    }

    Window::~Window()
    {
        ::SetWindowLongPtr(m_Win64Window, GWLP_USERDATA, 0);
        ::DestroyWindow(m_Win64Window);

        BENZIN_INFO("Window destoroyed");
    }

    Viewport Window::GetViewport() const
    {
        return Viewport
        {
            .X{ 0.0f },
            .Y{ 0.0f },
            .Width{ static_cast<float>(m_Width) },
            .Height{ static_cast<float>(m_Height) },
            .MinDepth{ 0.0f },
            .MaxDepth{ 1.0f }
        };
    }

    ScissorRect Window::GetScissorRect() const
    {
        return ScissorRect
        {
            .X{ 0.0f },
            .Y{ 0.0f },
            .Width{ static_cast<float>(m_Width) },
            .Height{ static_cast<float>(m_Height) }
        };
    }

    void Window::ProcessEvents()
    {
        MSG message{ nullptr };

        while (::PeekMessageW(&message, m_Win64Window, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }

    void Window::SetTitle(const std::string_view& title)
    {
        ::SetWindowText(m_Win64Window, title.data());
    }

    void Window::SetVisible(bool isVisible)
    {
        ::ShowWindow(m_Win64Window, isVisible ? SW_SHOW : SW_HIDE);
        ::UpdateWindow(m_Win64Window);
    }

    void Window::SetEventCallbackFunction(const EventCallbackFunction& callback)
    {
        m_EventCallback = callback;
    }

} // namespace benzin
