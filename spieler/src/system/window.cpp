#include "spieler/config/bootstrap.hpp"

#include "spieler/system/window.hpp"

namespace spieler
{

    Window::Window(const std::string& title, uint32_t width, uint32_t height)
        : m_Parameters
        {
            .Title = title,
            .Width = width,
            .Height = height
        }
    {}

    void Window::SetEventCallbackFunction(const EventCallbackFunction& callback)
    {
        m_EventCallback = callback;
    }

} // namespace spieler