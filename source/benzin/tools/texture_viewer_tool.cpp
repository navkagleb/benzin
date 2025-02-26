#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/texture_viewer_tool.hpp"

#include "benzin/graphics/texture.hpp"
#include "benzin/system/event.hpp"
#include "benzin/system/input.hpp"
#include "benzin/system/key_event.hpp"
#include "benzin/system/mouse_event.hpp"

namespace benzin
{

    static constexpr auto g_ToggleVisibilityKeyCode = KeyCode::F3;

    //

    TextureViewerTool::TextureViewerTool(const RenderResources& renderResources)
        , m_RenderResources{ renderResources }
        : ImGuiTool{ "TextureViewer", magic_enum::enum_name(g_ToggleVisibilityKeyCode) }
    {}

    void TextureViewerTool::OnEvent(Event& event)
    {
        const EventDispatcher dispatcher{ event };

        dispatcher.ForceDispatch<KeyPressedEvent>(&TextureViewerTool::OnKeyPressedEvent, this);

        if (m_IsHovered)
        {
            // Handle mouse events only when mouse hovers tool

            dispatcher.ForceDispatch<MouseMovedEvent>(&TextureViewerTool::OnMouseMovedEvent, this);
            dispatcher.ForceDispatch<MouseScrolledEvent>(&TextureViewerTool::OnMouseScrolledEvent, this);
        }
    }

    void TextureViewerTool::SpawnImGui()
    {
        ImGui::SetNextWindowBgAlpha(1.0);

        SpawnImGuiWindow([this]
        {
            const uint32_t textureIndex = m_SelectorCallback ? m_SelectorCallback() : g_InvalidUnsigned<uint32_t>;
            if (!IsValidUnsigned(textureIndex) || !m_Textures.IsCreated(textureIndex))
            {
                return;
            }

            const auto& texture = m_Textures.Get(textureIndex);

            const ImVec2 widgetSize = ImGui::GetContentRegionAvail();
            const float widgetAspectRatio = widgetSize.x / widgetSize.y;
            const float textureAspectRatio = (float)texture.GetWidth() / texture.GetHeight();

            ImVec2 widgetTextureSize{ 0.0f, 0.0f };
            if (widgetAspectRatio > textureAspectRatio)
            {
                widgetTextureSize.x = widgetSize.y * textureAspectRatio;
                widgetTextureSize.y = widgetSize.y;
            }
            else
            {
                widgetTextureSize.x = widgetSize.x;
                widgetTextureSize.y = widgetSize.x * (1.0f / textureAspectRatio);
            }

            ImGui::Text(BenzinFormatData("Uv Min: [{}, {}]", m_UvMin.x, m_UvMin.y));
            ImGui::Text(BenzinFormatData("Uv Max: [{}, {}]", m_UvMax.x, m_UvMax.y));
            ImGui::Text(BenzinFormatData("Texture Size: [{}, {}]", texture.GetWidth(), texture.GetHeight()));
            ImGui::Separator();

            ImGui::Image(
                ImGuiPass::PackImTextureId(texture.GetSrv(), joint::ImGuiSamplerIndex::Point),
                widgetTextureSize,
                m_UvMin,
                m_UvMax
            );

            m_IsHovered = ImGui::IsItemHovered();
        });
    }

    bool TextureViewerTool::OnKeyPressedEvent(const KeyPressedEvent& event)
    {
        if (event.GetKeyCode() == g_ToggleVisibilityKeyCode)
        {
            m_IsVisible = !m_IsVisible;
        }

        return true;
    }

    bool TextureViewerTool::OnMouseMovedEvent(const MouseMovedEvent& event)
    {
        if (!Input::IsMouseButtonPressed(MouseButton::Right))
        {
            Input::UnlockCursor();
            return true;
        }

        const DirectX::XMINT2 lockedCursorPosition = Input::LockCursor(*ms_Window);

        const float moveSpeed = 0.001f;
        const float offsetX = moveSpeed * (event.GetX() - lockedCursorPosition.x);
        const float offsetY = moveSpeed * (event.GetY() - lockedCursorPosition.y);

        const float width = m_UvMax.x - m_UvMin.x;
        const float height = m_UvMax.y - m_UvMin.y;

        const float newMinX = std::clamp(m_UvMin.x - offsetX, 0.0f, 1.0f - width);
        const float newMinY = std::clamp(m_UvMin.y - offsetY, 0.0f, 1.0f - height);

        m_UvMin.x = newMinX;
        m_UvMin.y = newMinY;
        m_UvMax.x = newMinX + width;
        m_UvMax.y = newMinY + height;

        return true;
    }

    bool TextureViewerTool::OnMouseScrolledEvent(const MouseScrolledEvent& event)
    {
        const float speed = 0.05f;
        const float scaleFactor = 1.0f + speed * -event.GetOffsetX(); // Scale up or down

        const float uvCenterX = (m_UvMin.x + m_UvMax.x) * 0.5f;
        const float uvCenterY = (m_UvMin.y + m_UvMax.y) * 0.5f;

        const float width = std::min((m_UvMax.x - m_UvMin.x) * scaleFactor, 1.0f);
        const float height = std::min((m_UvMax.y - m_UvMin.y) * scaleFactor, 1.0f);

        m_UvMin.x = std::clamp(uvCenterX - width * 0.5f, 0.0f, 1.0f - width);
        m_UvMin.y = std::clamp(uvCenterY - height * 0.5f, 0.0f, 1.0f - height);
        m_UvMax.x = m_UvMin.x + width;
        m_UvMax.y = m_UvMin.y + height;

        return true;
    }

}
