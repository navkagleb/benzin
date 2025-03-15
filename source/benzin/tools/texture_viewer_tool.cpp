#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/texture_viewer_tool.hpp"

#include "benzin/graphics/texture.hpp"
#include "benzin/graphics2/game_specific_resource_ids.hpp"
#include "benzin/graphics2/imgui_helpers.hpp"
#include "benzin/system/event.hpp"
#include "benzin/system/input.hpp"
#include "benzin/system/key_event.hpp"
#include "benzin/system/mouse_event.hpp"

namespace benzin
{

    static constexpr auto g_ToggleVisibilityKeyCode = KeyCode::F3;

    static TextureId DrawTextureSelector()
    {
        static const auto textureNames = magic_enum::enum_names<TextureId>();
        static const auto textureIndices = magic_enum::enum_values<TextureId>();

        static int textureNameIndex = -1;

        if (ImGui::Button("Reset"))
        {
            textureNameIndex = -1;
        }

        ImGui::SameLine();
        ImGui::Combo(
            "Texture",
            &textureNameIndex,
            ImGui_SelectComboName<decltype(textureNames)>,
            (void*)&textureNames,
            (int)textureNames.size() - 1
        );

        return textureNameIndex != -1 ? textureIndices[textureNameIndex] : g_InvalidTextureId;
    }

    //

    TextureViewerTool::TextureViewerTool(const RenderResources& resources)
        : ImGuiTool{ "TextureViewer", magic_enum::enum_name(g_ToggleVisibilityKeyCode) }
        , m_Resources{ resources }
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

        SpawnImGuiWindow(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, [this]
        {
            const TextureId textureId = DrawTextureSelector();
            if (!magic_enum::enum_contains(textureId) || !m_Resources.IsCreated(textureId))
            {
                return;
            }

            const auto& texture = m_Resources.Get(textureId);

            ImGui::Text(BenzinFormatData("Texture Size: [{}, {}]", texture.GetWidth(), texture.GetHeight()));

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
            const ImVec2 imagePos = ImGui::GetCursorScreenPos();

            ImGui::Separator();
            ImGui::Image(
                ImGuiPass::PackImTextureId(texture.GetSrv(), joint::ImGuiSamplerIndex::Point),
                widgetTextureSize,
                m_UvMin,
                m_UvMax
            );

            const float rounding = 0.0f;
            const float borderThickness = 3.0f;
            ImGui::GetWindowDrawList()->AddRect(
                imagePos,
                imagePos + widgetTextureSize,
                IM_COL32(255, 165, 0, 255),
                rounding,
                ImDrawFlags_None,
                borderThickness
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
