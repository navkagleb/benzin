#pragma once

#include <cstdint>
#include <string>

#include <Windows.h>

namespace Spieler
{

    class WindowsRegisterClass
    {
    public:
        inline const std::string& GetName() const { return m_Name; }

    public:
        bool Init();
        void Shutdown();

    private:
        const std::string m_Name = "SPIELER_WINDOW";

    private:
        WNDCLASSEX m_Info{};
    };

    class Window
    {
    public:
        inline HWND GetHandle() const { return m_Handle; }
        inline const std::string& GetTitle() const { return m_Title; }
        inline std::uint32_t GetWidth() const { return m_Width; }
        inline std::uint32_t GetHeight() const { return m_Height; }

    public:
        bool Init(const WindowsRegisterClass& windowsRegisterClass, const std::string& title, std::uint32_t width, std::uint32_t height);
        void Shutdown();

    private:
        HWND            m_Handle    = nullptr;
        std::string     m_Title     = "Default title";
        std::uint32_t   m_Width     = 0;
        std::uint32_t   m_Height    = 0;
    };

} // namespace Spieler
