#include "benzin/config/bootstrap.hpp"
#include "benzin/system/window.hpp"

#include <third_party/imgui/backends/imgui_impl_win32.h>

#include "benzin/system/window_event.hpp"
#include "benzin/system/mouse_event.hpp"
#include "benzin/system/key_event.hpp"

#include "benzin/graphics/common.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace benzin
{

    // Cannot move implementation of 'MessageHandle' and 'RegisterManager' to anonymous namespace
    // because 'MessageHandler' is a friend of 'Window'
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

        static uint32_t width = window->m_Width;
        static uint32_t height = window->m_Height;

        switch (messageCode)
        {
            // Window events
            case WM_CLOSE:
            {
                window->CreateAndPushEvent<WindowCloseEvent>();

                //::DestroyWindow(handle);

                break;
            }
            case WM_ACTIVATE:
            {
                if (LOWORD(wparam) == WA_INACTIVE)
                {
                    window->CreateAndPushEvent<WindowUnfocusedEvent>();
                }
                else
                {
                    window->CreateAndPushEvent<WindowFocusedEvent>();
                }

                break;
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

                        window->CreateAndPushEvent<WindowMinimizedEvent>();

                        break;
                    }
                    case SIZE_MAXIMIZED:
                    {
                        window->m_Width = width;
                        window->m_Height = height;
                        window->m_IsMinimized = false;
                        window->m_IsMaximized = true;

                        window->CreateAndPushEvent<WindowMaximizedEvent>();
                        window->CreateAndPushEvent<WindowResizedEvent>(window->m_Width, window->m_Height);

                        break;
                    }
                    case SIZE_RESTORED:
                    {
                        if (window->m_IsResizing)
                        {
                            window->CreateAndPushEvent<WindowResizingEvent>(window->m_Width, window->m_Height);
                        }
                        else
                        {
                            if (window->m_IsMinimized)
                            {
                                window->m_IsMinimized = false;
                            }
                            else if (window->m_IsMaximized)
                            {
                                window->m_IsMaximized = false;
                            }

                            window->m_Width = width;
                            window->m_Height = height;

                            window->CreateAndPushEvent<WindowRestoredEvent>();
                            window->CreateAndPushEvent<WindowResizedEvent>(window->m_Width, window->m_Height);
                        }

                        break;
                    }
                    default:
                    {
                        break;
                    }
                }

                break;
            }
            case WM_ENTERSIZEMOVE:
            {
                window->m_IsResizing = true;

                window->CreateAndPushEvent<WindowEnterResizingEvent>();

                break;
            }
            case WM_EXITSIZEMOVE:
            {
                window->m_IsResizing = false;

                window->CreateAndPushEvent<WindowExitResizingEvent>();

                if (window->m_Width != width || window->m_Height != height)
                {
                    window->m_Width = width;
                    window->m_Height = height;

                    window->CreateAndPushEvent<WindowResizedEvent>(window->m_Width, window->m_Height);
                }

                break;
            }
            case WM_GETMINMAXINFO:
            {
                reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.x = 200;
                reinterpret_cast<MINMAXINFO*>(lparam)->ptMinTrackSize.y = 200;

                break;
            }

            // Mouse events
            case WM_LBUTTONDOWN:
            {
                window->CreateAndPushEvent<MouseButtonPressedEvent>(MouseButton::Left, LOWORD(lparam), HIWORD(lparam));
                break;
            }
            case WM_RBUTTONDOWN:
            {
                window->CreateAndPushEvent<MouseButtonPressedEvent>(MouseButton::Right, LOWORD(lparam), HIWORD(lparam));
                break;
            }
            case WM_MBUTTONDOWN:
            {
                window->CreateAndPushEvent<MouseButtonPressedEvent>(MouseButton::Middle, LOWORD(lparam), HIWORD(lparam));
                break;
            }
            case WM_LBUTTONUP:
            {
                window->CreateAndPushEvent<MouseButtonReleasedEvent>(MouseButton::Left, LOWORD(lparam), HIWORD(lparam));
                break;
            }
            case WM_RBUTTONUP:
            {
                window->CreateAndPushEvent<MouseButtonReleasedEvent>(MouseButton::Right, LOWORD(lparam), HIWORD(lparam));
                break;
            }
            case WM_MBUTTONUP:
            {
                window->CreateAndPushEvent<MouseButtonReleasedEvent>(MouseButton::Middle, LOWORD(lparam), HIWORD(lparam));
                break;
            }
            case WM_MOUSEMOVE:
            {
                window->CreateAndPushEvent<MouseMovedEvent>(LOWORD(lparam), HIWORD(lparam));
                break;
            }
            case WM_MOUSEWHEEL:
            {
                window->CreateAndPushEvent<MouseScrolledEvent>(static_cast<int8_t>(GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1));
                break;
            }

            // Key event
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                window->CreateAndPushEvent<KeyPressedEvent>(static_cast<KeyCode>(wparam), static_cast<bool>(HIWORD(lparam) & KF_REPEAT));
                break;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                window->CreateAndPushEvent<KeyReleasedEvent>(static_cast<KeyCode>(wparam));
                break;
            }
            case WM_CHAR:
            case WM_SYSCHAR:
            {
                window->CreateAndPushEvent<KeyTypedEvent>(static_cast<char>(wparam));
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
        RegisterManager(std::string_view name, MessageHandler messageHandler)
            : m_Name{ name }
        {
            const WNDCLASSEX registerClass
            {
                .cbSize = sizeof(WNDCLASSEX),
                .style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC,
                .lpfnWndProc = messageHandler,
                .cbClsExtra = 0,
                .cbWndExtra = 0,
                .hInstance = ::GetModuleHandle(nullptr),
                .hIcon = ::LoadIcon(nullptr, IDI_APPLICATION),
                .hCursor = ::LoadCursor(nullptr, IDC_ARROW),
                .hbrBackground = nullptr,
                .lpszMenuName = nullptr,
                .lpszClassName = m_Name.data(),
                .hIconSm = ::LoadIcon(nullptr, IDI_APPLICATION),
            };

            BenzinAssert(::RegisterClassEx(&registerClass) != 0);
        }

        ~RegisterManager()
        {
            ::UnregisterClass(m_Name.data(), ::GetModuleHandle(nullptr));
        }

    public:
        std::string_view GetName() const { return m_Name; }

    private:
        std::string_view m_Name;
    };

    static RegisterManager g_RegisterManager{ "BENZIN_WINDOW_REGISTER_MANAGER", MessageHandler };

    Window::Window(std::string_view title, uint32_t width, uint32_t height, const EventCallbackFunction& eventCallback)
        : m_Width{ width }
        , m_Height{ height }
    {
        const LONG m_Style = WS_OVERLAPPEDWINDOW;

        RECT windowBounds
        {
            .left = 0,
            .top = 0,
            .right = static_cast<LONG>(m_Width),
            .bottom = static_cast<LONG>(m_Height),
        };

        BenzinAssert(::AdjustWindowRect(&windowBounds, m_Style, false) != 0);

        m_Win64Window = ::CreateWindow(
            g_RegisterManager.GetName().data(),
            title.data(),
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

        BenzinAssert(m_Win64Window);

        SetVisible(true);
        SetEventCallbackFunction(eventCallback);
    }

    Window::~Window()
    {
        ::SetWindowLongPtr(m_Win64Window, GWLP_USERDATA, 0);
        ::DestroyWindow(m_Win64Window);
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

    void Window::SetTitle(std::string_view title)
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
