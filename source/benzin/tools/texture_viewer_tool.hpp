#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class KeyPressedEvent;
    class MouseMovedEvent;
    class MouseScrolledEvent;
    class RenderResources;

    class TextureViewerTool : public ImGuiTool
    {
    public:
        friend class TextureViewerPass;
        friend class RenderViewportTool;

        explicit TextureViewerTool(const RenderResources& resources);

    private:
        enum class ColorChannel
        {
            R,
            G,
            B,
            A,
        };

        void OnEvent(Event& event) override;
        void DrawWindow() override;
        void DrawWindowContent() override;

        void DrawChannelCheckbox(const char* name, const ImVec4& textColor, const ImVec4& checkMarkColor, ColorChannel channel);
        void DrawShaderConsts(uint32_t textureDepth);
        void DrawDebugTexture() const;

        bool OnKeyPressedEvent(const KeyPressedEvent& event);
        bool OnMouseMovedEvent(const MouseMovedEvent& event);
        bool OnMouseScrolledEvent(const MouseScrolledEvent& event);

        bool IsReferenceTextureIdValid() const;

    private:
        const RenderResources& m_Resources;

        TextureId m_ReferenceTextureId = g_InvalidTextureId;

        bool m_IsFullViewportPreview = false;

        ImVec2 m_UvMin{ 0.0f, 0.0f };
        ImVec2 m_UvMax{ 1.0f, 1.0f };

        std::array<bool, magic_enum::enum_count<ColorChannel>()> m_IsChannelActive;
        float m_MinColor = 0.0f;
        float m_MaxColor = 1.0f;

        uint16_t m_ActiveDepthIndex = 0;
    };

}
