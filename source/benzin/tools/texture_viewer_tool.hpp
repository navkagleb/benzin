#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class KeyPressedEvent;
    class MouseMovedEvent;
    class MouseScrolledEvent;
    class RenderResources;

    class TextureViewerTool : public ImGuiTool
    {
    public:
        explicit TextureViewerTool(const RenderResources& resources);

    private:
        void OnEvent(Event& event) override;
        void SpawnImGui() override;

        bool OnKeyPressedEvent(const KeyPressedEvent& event);
        bool OnMouseMovedEvent(const MouseMovedEvent& event);
        bool OnMouseScrolledEvent(const MouseScrolledEvent& event);

    private:
        const RenderResources& m_Resources;

        ImVec2 m_UvMin{ 0.0f, 0.0f };
        ImVec2 m_UvMax{ 1.0f, 1.0f };

        bool m_IsHovered = false;
    };

}
