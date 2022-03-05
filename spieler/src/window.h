#pragma once

#include <cstdint>
#include <string>
#include <functional>

#define NOMINMAX
#include <Windows.h>

#include "event.h"

namespace Spieler
{

    class WindowRegisterClass
    {
    public:
        ~WindowRegisterClass();

    public:
        const std::string& GetName() const { return m_Name; }
        bool IsInitialized() const { return m_IsInitialized; }

    public:
        bool Init();

    private:
        const std::string m_Name = "SPIELER_WINDOW";

    private:
        WNDCLASSEX  m_Info{};
        bool        m_IsInitialized = false;
    };

    class Window
    {
    public:
        friend class WindowRegisterClass;

    public:
        using EventCallbackFunction = std::function<void(Event& event)>;

    private:
        static WindowRegisterClass& GetWindowRegisterClass();

    public:
        HWND GetHandle() const { return m_Handle; }
        const std::string& GetTitle() const { return m_Title; }
        std::uint32_t GetWidth() const { return m_Width; }
        std::uint32_t GetHeight() const { return m_Height; }

        float GetAspectRatio() const { return static_cast<float>(m_Width) / m_Height; }

    public:
        void SetTitle(const std::string& title);
        void SetEventCallbackFunction(const EventCallbackFunction& callback);

    public:
        bool Init(const std::string& title, std::uint32_t width, std::uint32_t height);
        
        bool Show();

    private:
        LONG                    m_Style         = WS_OVERLAPPEDWINDOW;
        HWND                    m_Handle        = nullptr;
        std::string             m_Title         = "Default title";
        std::uint32_t           m_Width         = 0;
        std::uint32_t           m_Height        = 0;
        EventCallbackFunction   m_EventCallback;
    };

} // namespace Spieler
