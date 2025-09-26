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
        ImGuiTool::DrawWindow();
        ImGui::PopStyleVar();

        m_Viewport.m_IsValidForRendering = !m_Viewport.m_IsResized && m_Viewport.m_Width != 0 && m_Viewport.m_Height != 0 && ImGuiTool::m_IsVisible; // TODO
    }

    void RenderViewportTool::DrawWindowContent()
    {
        UpdateViewportSize();

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
        
        const ImVec2 localCursorPosition = ImGui::GetMousePos() - ImGui::GetWindowPos();
        m_Viewport.m_CursorPosition.x = (int32_t)localCursorPosition.x;
        m_Viewport.m_CursorPosition.y = (int32_t)localCursorPosition.y;
    }

    void RenderViewportTool::UpdateViewportSize()
    {
        m_Viewport.m_IsResized = false;

        if (ImGui::IsAnyItemActive()) // In resizing state
            return;

        const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        if ((uint32_t)viewportSize.x == m_Viewport.GetWidth() && (uint32_t)viewportSize.y == m_Viewport.GetHeight())
            return;

        if (ImGui::IsWindowAppearing() || ImGui::IsWindowCollapsed())
            return;

        m_Viewport.m_Width = (uint32_t)viewportSize.x;
        m_Viewport.m_Height = (uint32_t)viewportSize.y;
        m_Viewport.m_IsResized = true;
    }

}
