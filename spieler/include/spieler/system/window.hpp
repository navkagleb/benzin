#pragma once

#include "spieler/system/event.hpp"
#include "spieler/system/input.hpp"

namespace spieler
{

    class Window
    {
    public:
        using EventCallbackFunction = std::function<void(Event& event)>;

    public:
        Window(const std::string& title, uint32_t width, uint32_t height);
        virtual ~Window() = default;

    public:
        template <typename T = void*>
        T GetNativeHandle() { return static_cast<T>(m_NativeHandle); }

        const Input& GetInput() const { return m_Input; }

        const std::string& GetTitle() const { return m_Parameters.Title; }
        uint32_t GetWidth() const { return m_Parameters.Width; }
        uint32_t GetHeight() const { return m_Parameters.Height; }

        bool IsResizing() const { return m_Parameters.IsResizing; }
        bool IsMinimized() const { return m_Parameters.IsMinimized; }
        bool IsMaximized() const { return m_Parameters.IsMaximized; }

        float GetAspectRatio() const { return static_cast<float>(m_Parameters.Width) / m_Parameters.Height; }

    public:
        void ProcessEvents();

        void SetTitle(const std::string& title);
        void SetVisible(bool isVisible);

        void SetEventCallbackFunction(const EventCallbackFunction& callback);

    protected:
        struct Parameters
        {
            std::string Title{ "Default title" };
            uint32_t Width{ 0 };
            uint32_t Height{ 0 };
            bool IsResizing{ false };
            bool IsMinimized{ false };
            bool IsMaximized{ false };
        };

    protected:
        void* m_NativeHandle{ nullptr };
        Parameters m_Parameters;
        Input m_Input;
        EventCallbackFunction m_EventCallback;
    };

} // namespace spieler
