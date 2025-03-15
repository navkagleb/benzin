#pragma once

#include "benzin/engine/camera.hpp"
#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class MouseMovedEvent;
    class MouseScrolledEvent;
    class RenderResources;

    class RenderViewportTool : public ImGuiTool
    {
    public:
        friend class FlyCameraController;

        RenderViewportTool(RenderResources& resources, Camera& camera);

        auto& GetFlyCameraController() { return m_FlyCameraController; }

        uint32_t GetWidth() const { return (uint32_t)m_ViewportSize.x; }
        uint32_t GetHeight() const { return (uint32_t)m_ViewportSize.y; }

        bool IsViewportSizeRelevant() const { return m_IsViewportSizeRelevant; }
        bool IsValidForRendering() const { return m_IsViewportSizeRelevant && m_IsVisible; }

    private:
        void OnEvent(Event& event) override;
        void SpawnImGui() override;

        void UpdateImGuiDimensions();

        bool OnMouseMovedEvent(const MouseMovedEvent& event);
        bool OnMouseScrolledEvent(const MouseScrolledEvent& event);

    private:
        RenderResources& m_Resources;
        FlyCameraController m_FlyCameraController;
        
        DirectX::XMINT2 m_ViewportSize{};
        bool m_IsViewportSizeRelevant = true;

        bool m_IsViewportHovered = false;
        bool m_IsViewportActive = false;
    };

}
