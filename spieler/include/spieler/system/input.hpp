#pragma once

#include "spieler/system/mouse_button.hpp"
#include "spieler/system/key_code.hpp"

namespace spieler
{

    class InputPlatform
    {
    public:
        virtual ~InputPlatform() = default;

    public:
        virtual float GetMouseX() const = 0;
        virtual float GetMouseY() const = 0;

        virtual bool IsMouseButtonPressed(MouseButton mouseButton) const = 0;
        virtual bool IsKeyPressed(KeyCode keyCode) const = 0;
    };

    class Input : private InputPlatform
    {
    public:
        float GetMouseX() const { return m_InputPlatform->GetMouseX(); }
        float GetMouseY() const { return m_InputPlatform->GetMouseY(); };

        bool IsMouseButtonPressed(MouseButton mouseButton) const { return m_InputPlatform->IsMouseButtonPressed(mouseButton); }
        bool IsKeyPressed(KeyCode keyCode) const { return m_InputPlatform->IsKeyPressed(keyCode); }

    public:
        void SetInputPlatform(std::unique_ptr<InputPlatform>&& inputPlatform) { m_InputPlatform = std::move(inputPlatform); }

    private:
        std::unique_ptr<InputPlatform> m_InputPlatform;
    };

} // namespace spieler