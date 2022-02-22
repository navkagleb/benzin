#include "window.h"

namespace Spieler
{

    bool WindowsRegisterClass::Init(EventCallbackFunction eventCallbackFunction)
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
        m_Info.lpfnWndProc     = eventCallbackFunction;

        const auto status = ::RegisterClassEx(&m_Info);

        if (!status)
        {
            return false;
        }

        return true;
    }

    void WindowsRegisterClass::Shutdown()
    {
        ::UnregisterClass(m_Name.c_str(), ::GetModuleHandle(nullptr));
    }

    void Window::SetTitle(const std::string& title)
    {
        ::SetWindowText(m_Handle, title.c_str());
    }

    bool Window::Init(const WindowsRegisterClass& windowsRegisterClass, const std::string& title, std::uint32_t width, std::uint32_t height)
    {
        m_Title     = title;
        m_Width     = width;
        m_Height    = height;

        const auto  style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        RECT        windowBounds = { 0, 0, static_cast<std::int32_t>(m_Width), static_cast<std::int32_t>(m_Height) };

        auto status = ::AdjustWindowRect(&windowBounds, style, false);

        if (!status)
        {
            return false;
        }

        m_Handle = ::CreateWindow(
            windowsRegisterClass.GetName().c_str(),
            m_Title.c_str(),
            style,
            (::GetSystemMetrics(SM_CXSCREEN) - windowBounds.right) / 2,
            (::GetSystemMetrics(SM_CYSCREEN) - windowBounds.bottom) / 2,
            windowBounds.right - windowBounds.left,
            windowBounds.bottom - windowBounds.top,
            nullptr,
            nullptr,
            ::GetModuleHandle(nullptr),
            this
        );

        if (m_Handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        ::ShowWindow(m_Handle, SW_SHOWDEFAULT);

        status = ::UpdateWindow(m_Handle);

        if (!status)
        {
            return false;
        }

        return true;
    }

    void Window::Shutdown()
    {

    }

} // namespace Spieler