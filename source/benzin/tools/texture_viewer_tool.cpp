#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/texture_viewer_tool.hpp"

#include "benzin/graphics/texture.hpp"
#include "benzin/graphics2/game_specific_resource_ids.hpp"
#include "benzin/system/event.hpp"
#include "benzin/system/input.hpp"
#include "benzin/system/key_event.hpp"
#include "benzin/system/mouse_event.hpp"

BenzinEnableUnaryPlusForEnum(benzin::TextureViewerTool::ColorChannel);

namespace benzin
{

    static constexpr auto g_ToggleVisibilityKeyCode = KeyCode::F3;

    static TextureId DrawTextureSelector()
    {
        static const auto textureNames = magic_enum::enum_names<TextureId>();
        static const auto textureIndices = magic_enum::enum_values<TextureId>();

        static int textureNameIndex = 0;

        if (ImGui::Button("Reset"))
        {
            textureNameIndex = -1;
        }

        ImGui::SameLine();
        ImGui::Combo(
            "Texture",
            &textureNameIndex,
            ImGui::SelectComboName<decltype(textureNames)>,
            (void*)&textureNames,
            (int)textureNames.size() - 1
        );

        return textureNameIndex != -1 ? textureIndices[textureNameIndex] : g_InvalidTextureId;
    }

    static void DrawTextureConfig(const Texture& texture)
    {
        ImGui::Text(BenzinFormatData("Texture size: [{}, {}, {}]", texture.GetWidth(), texture.GetHeight(), texture.GetDepth()));
        ImGui::Text(BenzinFormatData("Texture mips: {}", texture.GetMipCount()));
        ImGui::Text(BenzinFormatData("Texture format: {}", magic_enum::enum_name(texture.GetFormat())));
    }

    //

    TextureViewerTool::TextureViewerTool(const RenderResources& resources)
        : ImGuiTool{ "Debug/TextureViewer", KeyCode::F3 }
        , m_Resources{ resources }
    {
        m_IsChannelActive[+ColorChannel::R] = true;
        m_IsChannelActive[+ColorChannel::G] = true;
        m_IsChannelActive[+ColorChannel::B] = true;
        m_IsChannelActive[+ColorChannel::A] = false;
    }

    void TextureViewerTool::OnEvent(Event& event)
    {
        ImGuiTool::OnEvent(event);


        if (m_IsHovered)
        {
            // Handle mouse events only when mouse hovers tool

            const EventDispatcher dispatcher{ event };
            dispatcher.ForceDispatch<MouseMovedEvent>(&TextureViewerTool::OnMouseMovedEvent, this);
            dispatcher.ForceDispatch<MouseScrolledEvent>(&TextureViewerTool::OnMouseScrolledEvent, this);
        }
    }

    void TextureViewerTool::DrawWindow()
    {
        ImGui::SetNextWindowBgAlpha(1.0);
        ImGuiTool::DrawWindow(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    void TextureViewerTool::DrawWindowContent()
    {
        const TextureId selectedReferenceTexture = DrawTextureSelector();

        if (m_ReferenceTextureId != selectedReferenceTexture)
        {
            m_ReferenceTextureId = selectedReferenceTexture;
            m_ActiveDepthIndex = 0;
            m_ActiveMipIndex = 0;
        }

        if (!IsReferenceTextureIdValid())
        {
            return;
        }

        const auto& texture = m_Resources.Get(m_ReferenceTextureId);

        ImGui::Checkbox("Full Viewport Preview", &m_IsFullViewportPreview);

        ImGui::CollapsingHeaderWithIndent("Texture Config", [this, &texture]
        {
            DrawTextureConfig(texture);
        }, ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::CollapsingHeaderWithIndent("Shader Consts", [this, &texture]
        {
            DrawChannelCheckbox("R", ImVec4{ 1.0f, 0.0f, 0.0f, 1.0f }, ImVec4{ 1.0f, 0.0f, 0.0f, 0.8f }, ColorChannel::R);
            DrawChannelCheckbox("G", ImVec4{ 0.0f, 1.0f, 0.0f, 1.0f }, ImVec4{ 0.0f, 1.0f, 0.0f, 0.8f }, ColorChannel::G);
            DrawChannelCheckbox("B", ImVec4{ 0.0f, 0.0f, 1.0f, 1.0f }, ImVec4{ 0.0f, 0.0f, 1.0f, 0.8f }, ColorChannel::B);
            DrawChannelCheckbox("A", ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f }, ImVec4{ 1.0f, 1.0f, 1.0f, 0.8f }, ColorChannel::A);
            DrawShaderConsts(texture.GetDepth(), texture.GetMipCount());
        }, ImGuiTreeNodeFlags_DefaultOpen);

        DrawDebugTexture();
    }

    void TextureViewerTool::DrawChannelCheckbox(const char* name, const ImVec4& textColor, const ImVec4& checkMarkColor, ColorChannel channel)
    {
        const bool isRgb = channel != ColorChannel::A;
        const bool isAnyRgbActive = m_IsChannelActive[+ColorChannel::R] || m_IsChannelActive[+ColorChannel::G] || m_IsChannelActive[+ColorChannel::B];
        const bool isAlphaActive = m_IsChannelActive[+ColorChannel::A];
        const bool isActive = (isRgb && !isAlphaActive) || (!isRgb && !isAnyRgbActive);

        ImGui::BeginDisabled(!isActive);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, checkMarkColor);
        ImGui::Checkbox(name, &m_IsChannelActive[+channel]);
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        ImGui::EndDisabled();
    }

    void TextureViewerTool::DrawShaderConsts(uint32_t textureDepth, uint32_t mipCount)
    {
        ImGui::PushItemWidth(200.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::NewLine();

        {
            ImGui::PushID(0);
            BenzinExecuteOnScopeExit([] { ImGui::PopID(); });

            if (ImGui::Button("Reset to 0"))
            {
                m_MinColor = 0.0f;
            }

            ImGui::SameLine();
            ImGui::DragFloat("Min color", &m_MinColor, 0.001f, std::numeric_limits<float>::lowest(), m_MaxColor, "%.3f", ImGuiSliderFlags_ClampOnInput);
        }

        {
            ImGui::PushID(1);
            BenzinExecuteOnScopeExit([] { ImGui::PopID(); });

            if (ImGui::Button("Reset to 1"))
            {
                m_MaxColor = 1.0f;
            }

            ImGui::SameLine();
            ImGui::DragFloat("Max color", &m_MaxColor, 0.001f, m_MinColor, std::numeric_limits<float>::max(), "%.3f", ImGuiSliderFlags_ClampOnInput);
        }

        {
            ImGui::PushID(2);
            BenzinExecuteOnScopeExit([] { ImGui::PopID(); });

            if (ImGui::Button("Reset to 0"))
            {
                m_ActiveDepthIndex = 0;
            }

            ImGui::SameLine();
            ImGui::SliderInt("Active depth index", (int*)&m_ActiveDepthIndex, 0, textureDepth - 1);
        }

        {
            ImGui::PushID(3);
            BenzinExecuteOnScopeExit([] { ImGui::PopID(); });

            if (ImGui::Button("Reset to 0"))
            {
                m_ActiveMipIndex = 0;
            }

            ImGui::SameLine();
            ImGui::SliderInt("Active mip index", (int*)&m_ActiveMipIndex, 0, mipCount - 1);
        }
    }

    void TextureViewerTool::DrawDebugTexture() const
    {
        if (!m_Resources.IsCreated(TextureId::DebugTexture))
        {
            return;
        }

        const Texture& debugTexture = m_Resources.Get(TextureId::DebugTexture);

        ImGui::Text(BenzinFormatData("Debug texture size: [{}, {}]", debugTexture.GetWidth(), debugTexture.GetHeight()));

        const ImVec2 widgetSize = ImGui::GetContentRegionAvail();
        const float widgetAspectRatio = widgetSize.x / widgetSize.y;
        const float textureAspectRatio = (float)debugTexture.GetWidth() / debugTexture.GetHeight();

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

        ImGui::Image(
            ImGuiPass::PackImTextureId(debugTexture.GetSrv(), joint::ImGuiSamplerIndex::Point),
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

    bool TextureViewerTool::IsReferenceTextureIdValid() const
    {
        return m_ReferenceTextureId != g_InvalidTextureId && m_Resources.IsCreated(m_ReferenceTextureId);
    }

}
