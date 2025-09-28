#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/render_viewport_tool.hpp>

#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{

    RenderViewportTool::RenderViewportTool(RenderViewport& viewport, RenderResources& resources)
        : ImGuiTool{ "Graphics/RenderViewport" }
        , m_Viewport{ viewport }
        , m_Resources{ resources }
    {}

    void RenderViewportTool::DrawWindow()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGuiTool::DrawWindow(ImGuiWindowFlags_None);
        ImGui::PopStyleVar();
    }

    void RenderViewportTool::DrawWindowContent()
    {
        if (!UpdateViewportSize())
            return;

        if (!m_Resources.IsCreated(m_Viewport.m_DisplayTextureId))
            return;

        const Texture& viewportTexture = m_Resources.Get(m_Viewport.m_DisplayTextureId);

        ImVec2 imageSize;
        imageSize.x = (float)m_Viewport.GetWidth();
        imageSize.y = (float)m_Viewport.GetHeight();

        if (viewportTexture.GetWidth() != m_Viewport.GetWidth() || viewportTexture.GetHeight() != m_Viewport.GetHeight())
        {
            const ImVec2 cursorPosition = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                cursorPosition,
                cursorPosition + imageSize,
                IM_COL32(50, 50, 50, 255));

            const float viewportAspectRatio = imageSize.x / imageSize.y;
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
            imageSize);

        m_Viewport.m_IsHovered = ImGui::IsItemHovered();
    }

    void RenderViewportTool::PostDrawWindow()
    {
        m_Viewport.m_IsValidForRendering = ImGuiTool::m_IsVisible && m_Viewport.GetWidth() != 0 && m_Viewport.GetHeight() != 0;
    }

    bool RenderViewportTool::UpdateViewportSize()
    {
        if (ImGui::IsAnyItemActive()) // In resizing state
            return true;

        if (ImGui::IsWindowAppearing() || ImGui::IsWindowCollapsed())
            return false;

        const ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x == m_Viewport.GetWidth() && size.y == m_Viewport.GetHeight())
            return true;

        if (size.x != 0 && size.y != 0)
        {
            m_Viewport.DeferResize((uint32_t)size.x, (uint32_t)size.y);
        }

        return false;
    }

}
