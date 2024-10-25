#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/render_viewport_tool.hpp"

#include "benzin/graphics/texture.hpp"
#include "benzin/system/event.hpp"
#include "benzin/system/mouse_event.hpp"

namespace benzin
{

    RenderViewportTool::RenderViewportTool(RenderResources& renderResources, Camera& camera)
        : ImGuiTool{ "RenderViewportTool", true }
        , m_RenderResources{ renderResources }
        , m_FlyCameraController{ camera }
    {}

    void RenderViewportTool::OnEvent(Event& event)
    {
        if (!m_IsViewportHovered)
        {
            return;
        }

        const EventDispatcher dispatcher{ event };

        dispatcher.ForceDispatch<MouseMovedEvent>([this](const auto& event)
        {
            return m_FlyCameraController.OnMouseMoved(event);
        });

        dispatcher.ForceDispatch<MouseScrolledEvent>([this](const auto& event)
        {
            return m_FlyCameraController.OnMouseScrolled(event);
        });
    }

    void RenderViewportTool::SpawnImGui()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

        SpawnImGuiWindow([this]
        {
            UpdateImGuiDimensions();

            if (IsValidUnsigned(m_FinalTextureIndex))
            {
                const auto* finalTexture = m_RenderResources.GetTexturePtr(m_FinalTextureIndex);
                if (finalTexture != nullptr)
                {
                    ImGui::Image((ImTextureID)finalTexture->GetSrv().GetGpuHandle(), ImVec2
                    {
                        (float)finalTexture->GetWidth(),
                        (float)finalTexture->GetHeight(),
                    });

                    m_IsViewportHovered = ImGui::IsItemHovered();
                }
            }
        });

        ImGui::PopStyleVar();
    }

    void RenderViewportTool::UpdateImGuiDimensions()
    {
        m_IsViewportSizeRelevant = true;

        const DirectX::XMINT2 viewportSize
        {
            (int32_t)ImGui::GetContentRegionAvail().x,
            (int32_t)ImGui::GetContentRegionAvail().y,
        };

        const bool isInResizingState = ImGui::IsAnyItemActive();
        const bool isEqual = viewportSize.x == m_ViewportSize.x && viewportSize.y == m_ViewportSize.y;
        const bool isCollapsed = viewportSize.x <= 0 || viewportSize.y <= 0;
        if (isInResizingState || isEqual || isCollapsed)
        {
            return;
        }

        m_ViewportSize = viewportSize;
        m_IsViewportSizeRelevant = false;

        m_FlyCameraController.OnRenderViewportResized(m_ViewportSize.x, m_ViewportSize.y);
    }

}
