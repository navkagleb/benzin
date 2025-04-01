#pragma once

#include "benzin/engine/camera.hpp"
#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class MouseMovedEvent;
    class MouseScrolledEvent;
    class RenderResources;
    class TextureViewerTool;

    class RenderViewportTool : public ImGuiTool
    {
    public:
        friend class FlyCameraController;
        friend class FlyCameraTool;

        RenderViewportTool(RenderResources& resources, TextureViewerTool& textureViewerTool, Camera& camera);

        uint32_t GetWidth() const { return (uint32_t)m_ViewportSize.x; }
        uint32_t GetHeight() const { return (uint32_t)m_ViewportSize.y; }

        bool IsViewportSizeValid() const { return m_IsViewportSizeValid; }
        bool IsValidForRendering() const { return m_IsViewportSizeValid && ImGuiTool::m_IsVisible; }

        void MoveCamera(std::chrono::microseconds dt);

    private:
        void OnEvent(Event& event) override;
        void DrawWindow() override;
        void DrawWindowContent() override;

        void UpdateImGuiDimensions();

        bool OnMouseMovedEvent(const MouseMovedEvent& event);
        bool OnMouseScrolledEvent(const MouseScrolledEvent& event);

    private:
        RenderResources& m_Resources;
        TextureViewerTool& m_TextureViewerTool;

        FlyCameraController m_FlyCameraController;

        ImVec2 m_ViewportSize{};
        bool m_IsViewportSizeValid = true;
        bool m_IsViewportHovered = false;
    };

}
