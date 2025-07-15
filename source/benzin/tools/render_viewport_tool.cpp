#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/render_viewport_tool.hpp"

#include "benzin/graphics/texture.hpp"
#include "benzin/graphics2/game_specific_resource_ids.hpp"
#include "benzin/graphics2/render_pass.hpp"
#include "benzin/system/event.hpp"
#include "benzin/system/input.hpp"
#include "benzin/system/mouse_event.hpp"
#include "benzin/tools/texture_viewer_tool.hpp"

namespace benzin
{

    RenderViewportTool::RenderViewportTool(RenderResources& resources, TextureViewerTool& textureViewerTool, Camera& camera)
        : ImGuiTool{ "Graphics/RenderViewport" }
        , m_Resources{ resources }
        , m_TextureViewerTool{ textureViewerTool }
        , m_FlyCameraController{ camera }
    {}

    void RenderViewportTool::MoveCamera(std::chrono::microseconds dt)
    {
        m_FlyCameraController.MoveCamera(dt);
    }

    void RenderViewportTool::OnEvent(Event& event)
    {
        if (!m_IsViewportHovered)
        {
            return;
        }

        const EventDispatcher dispatcher{ event };

        dispatcher.ForceDispatch<MouseMovedEvent>(&RenderViewportTool::OnMouseMovedEvent, this);
        dispatcher.ForceDispatch<MouseScrolledEvent>(&RenderViewportTool::OnMouseScrolledEvent, this);
    }

    void RenderViewportTool::DrawWindow()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGuiTool::DrawWindow();
        ImGui::PopStyleVar();
    }

    void RenderViewportTool::DrawWindowContent()
    {
        UpdateImGuiDimensions();

        const auto viewportTextureId = m_TextureViewerTool.m_IsFullViewportPreview ? TextureId::DebugTexture : TextureId::Final;
        if (!m_Resources.IsCreated(viewportTextureId))
        {
            return;
        }

        const auto& viewportTexture = m_Resources.Get(viewportTextureId);

        ImVec2 imageSize = m_ViewportSize;
        if (viewportTexture.GetWidth() != m_ViewportSize.x || viewportTexture.GetHeight() != m_ViewportSize.y)
        {
            const ImVec2 cursorPosition = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                cursorPosition,
                cursorPosition + m_ViewportSize,
                IM_COL32(50, 50, 50, 255)
            );

            const float viewportAspectRatio = m_ViewportSize.x / m_ViewportSize.y;
            const float textureAspectRatio = (float)viewportTexture.GetWidth() / viewportTexture.GetHeight();

            if (viewportAspectRatio > textureAspectRatio)
            {
                imageSize.x = imageSize.y * textureAspectRatio;
            }
            else
            {
                imageSize.y = imageSize.x / textureAspectRatio;
            }
        }

        ImGui::Image(
            ImGuiPass::PackImTextureId(viewportTexture.GetSrv(), joint::ImGuiSamplerIndex::Point),
            imageSize
        );

        m_IsViewportHovered = ImGui::IsItemHovered();
    }

    void RenderViewportTool::UpdateImGuiDimensions()
    {
        m_IsViewportSizeValid = true;

        const ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        const bool isInResizingState = ImGui::IsAnyItemActive();
        const bool isEqual = viewportSize == m_ViewportSize;
        if (isInResizingState || isEqual || ImGui::IsWindowAppearing() || ImGui::IsWindowCollapsed())
        {
            return;
        }

        m_ViewportSize = viewportSize;
        m_IsViewportSizeValid = false;

        m_FlyCameraController.OnRenderViewportResized((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
    }

    bool RenderViewportTool::OnMouseMovedEvent(const MouseMovedEvent& event)
    {
        if (!Input::IsMouseButtonPressed(MouseButton::Right))
        {
            Input::UnlockCursor();
            return true;
        }

        const DirectX::XMINT2 mousePosition = event.GetPosition();
        const DirectX::XMINT2 lockedCursorPosition = Input::LockCursor(*ms_Window);

        m_FlyCameraController.RotateCamera(mousePosition, lockedCursorPosition);

        return true;
    }

    bool RenderViewportTool::OnMouseScrolledEvent(const MouseScrolledEvent& event)
    {
        m_FlyCameraController.IncrementFov((float)event.GetOffsetX());

        return true;
    }


}
