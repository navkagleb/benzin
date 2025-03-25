#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/render_viewport_tool.hpp"

#include "benzin/graphics/texture.hpp"
#include "benzin/graphics2/game_specific_resource_ids.hpp"
#include "benzin/graphics2/render_pass.hpp"
#include "benzin/system/event.hpp"
#include "benzin/system/input.hpp"
#include "benzin/system/mouse_event.hpp"

namespace benzin
{

    RenderViewportTool::RenderViewportTool(RenderResources& resources, Camera& camera)
        : ImGuiTool{ "RenderViewport" }
        , m_Resources{ resources }
        , m_FlyCameraController{ camera }
    {}

    void RenderViewportTool::MoveCamera(std::chrono::microseconds dt)
    {
        if (m_IsViewportHovered)
        {
            m_FlyCameraController.MoveCamera(dt);
        }
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

        if (!m_Resources.IsCreated(TextureId::Final))
        {
            return;
        }

        const auto& finalTexture = m_Resources.Get(TextureId::Final);
        const auto imTextureId = ImGuiPass::PackImTextureId(finalTexture.GetSrv(), joint::ImGuiSamplerIndex::Point);

        ImGui::Image(imTextureId, ImVec2
        {
            (float)finalTexture.GetWidth(),
            (float)finalTexture.GetHeight(),
        });

        m_IsViewportHovered = ImGui::IsItemHovered();
    }

    void RenderViewportTool::UpdateImGuiDimensions()
    {
        m_IsViewportSizeValid = true;

        const DirectX::XMINT2 viewportSize
        {
            (int32_t)ImGui::GetContentRegionAvail().x,
            (int32_t)ImGui::GetContentRegionAvail().y,
        };

        const bool isInResizingState = ImGui::IsAnyItemActive();
        const bool isEqual = viewportSize.x == m_ViewportSize.x && viewportSize.y == m_ViewportSize.y;
        if (isInResizingState || isEqual || ImGui::IsWindowAppearing() || ImGui::IsWindowCollapsed())
        {
            return;
        }

        m_ViewportSize = viewportSize;
        m_IsViewportSizeValid = false;

        m_FlyCameraController.OnRenderViewportResized(m_ViewportSize.x, m_ViewportSize.y);
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
