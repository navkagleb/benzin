#pragma once

#if defined(SPIELER_PLATFORM_WINDOWS)

#include "spieler/system/window.hpp"

namespace spieler
{

    class Win64_RegisterClass
    {
    public:
        typedef LRESULT(*MessageHandler)(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    public:
        static Win64_RegisterClass& GetInstance();

    public:
        Win64_RegisterClass(MessageHandler messageHandler);
        ~Win64_RegisterClass();

    public:
        const std::string& GetName() const { return m_Name; }

    private:
        const std::string m_Name{ "SPIELER_WINDOW" };
        ::WNDCLASSEX m_Info{};
    };

    class Win64_Window : public Window
    {
    public:
        friend class Window;
        friend class Win64_RegisterClass;

    public:
        static LRESULT MessageHandler(::HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    public:
        Win64_Window(const std::string& title, uint32_t width, uint32_t height);
        ~Win64_Window() override;

    private:
        ::LONG m_Style{ WS_OVERLAPPEDWINDOW };
    };

} // namespace spieler

#endif // defined(SPIELER_WIN64)