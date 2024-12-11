#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/texture_viewer_tool.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/engine/render_pass.hpp"
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
        : ImGuiTool{ "TextureViewerTool", magic_enum::enum_name(g_ToggleVisibilityKeyCode) }
        , m_RenderResources{ renderResources }
    {}

    void TextureViewerTool::OnEvent(Event& event)
    {
        const EventDispatcher dispatcher{ event };

        dispatcher.ForceDispatch<KeyPressedEvent>([this](const auto& event)
        {
            if (event.GetKeyCode() == g_ToggleVisibilityKeyCode)
            {
                m_IsVisible = !m_IsVisible;
            }

            return true;
        });

        if (!m_IsHovered)
        {
            // Handled mouse events only when mouse hovers tool
            return;
        }

        dispatcher.ForceDispatch<MouseScrolledEvent>([this](const auto& event)
        {
            const float speed = 0.01f;
            const float minScaleDelta = 0.021f;

            const bool isWithinMinScaleDelta = std::abs(m_UvMin.x - m_UvMax.x) <= minScaleDelta || std::abs(m_UvMin.y - m_UvMax.y) <= minScaleDelta;
            const bool isOffsetXPositive = event.GetOffsetX() > 0;

            const bool isScalingAllowed = !(isWithinMinScaleDelta && isOffsetXPositive);
            if (isScalingAllowed)
            {
                m_UvMin.x += speed * event.GetOffsetX();
                m_UvMin.y += speed * event.GetOffsetX();

                m_UvMax.x -= speed * event.GetOffsetX();
                m_UvMax.y -= speed * event.GetOffsetX();

                ClampUvs();
            }

            return true;
        });

        dispatcher.ForceDispatch<MouseMovedEvent>([this](const auto& event)
        {
            if (!Input::IsMouseButtonPressed(MouseButton::Right))
            {
                Input::UnlockCursor();
                return true;
            }

            const DirectX::XMINT2 lockedCursorPosition = Input::LockCursor(*ms_Window);

            const float deltaX = event.GetX() - lockedCursorPosition.x;
            const float deltaY = event.GetY() - lockedCursorPosition.y;

            const bool isMovementOnXAxisAllowed = !((m_UvMin.x == 0.0f && deltaX > 0.0f) || (m_UvMax.x == 1.0f && deltaX < 0.0f));
            const bool isMovementOnYAxisAllowed = !((m_UvMin.y == 0.0f && deltaY > 0.0f) || (m_UvMax.y == 1.0f && deltaY < 0.0f));

            const float speed = 0.001f;

            if (isMovementOnXAxisAllowed)
            {
                m_UvMin.x -= speed * deltaX;
                m_UvMax.x -= speed * deltaX;
            }

            if (isMovementOnYAxisAllowed)
            {
                m_UvMin.y -= speed * deltaY;
                m_UvMax.y -= speed * deltaY;
            }

            ClampUvs();

            return true;
        });
    }

    void TextureViewerTool::SpawnImGui()
    {
        ImGui::SetNextWindowBgAlpha(1.0);

        SpawnImGuiWindow([this]
        {
            const uint32_t textureIndex = m_SelectorCallback ? m_SelectorCallback() : g_InvalidUnsigned<uint32_t>;
            if (!IsValidUnsigned(textureIndex))
            {
                return;
            }

            const Texture* texture = m_RenderResources.GetTexturePtr(textureIndex);
            if (texture == nullptr)
            {
                return;
            }

            ImGui::Text(BenzinFormatData("Uv Min: [{}, {}]", m_UvMin.x, m_UvMin.y));
            ImGui::Text(BenzinFormatData("Uv Max: [{}, {}]", m_UvMax.x, m_UvMax.y));
            ImGui::ColorEdit4("Tint Color", &m_TintColor.x);
            ImGui::Separator();

            const ImVec2 widgetSize = ImGui::GetContentRegionAvail();

            const float widgetAspectRatio = widgetSize.x / widgetSize.y;
            const float textureAspectRatio = (float)texture->GetWidth() / texture->GetHeight();

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

            // Set custom callback with point sampler
            // ImDrawCallback callback;
            // ImGui::GetWindowDrawList()->AddCallback(callback, nullptr);

            ImGui::Image(
                (ImTextureID)texture->GetSrv().GetGpuHandle(),
                widgetTextureSize,
                m_UvMin,
                m_UvMax,
                m_TintColor
            );

            m_IsHovered = ImGui::IsItemHovered();
        });
    }

    void TextureViewerTool::ClampUvs()
    {
        m_UvMin.x = std::clamp(m_UvMin.x, 0.0f, m_UvMax.x);
        m_UvMin.y = std::clamp(m_UvMin.y, 0.0f, m_UvMax.y);

        m_UvMax.x = std::clamp(m_UvMax.x, m_UvMin.x, 1.0f);
        m_UvMax.y = std::clamp(m_UvMax.y, m_UvMin.y, 1.0f);

#if 0
        const float xDelta = std::abs(m_UvMin.x - m_UvMax.x);
        const float yDelta = std::abs(m_UvMin.y - m_UvMax.y);

        if (std::abs(xDelta - yDelta) > std::numeric_limits<float>::epsilon())
        {
            const auto [minDelta, maxDelta] = std::minmax(xDelta, yDelta);
            const float delta = maxDelta - minDelta;

            if (xDelta)

            if (m_UvMin.x == 0.0f)
            {

            }
        }
#endif
    }

}
