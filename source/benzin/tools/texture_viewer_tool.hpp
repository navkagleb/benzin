#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class KeyPressedEvent;
    class MouseMovedEvent;
    class MouseScrolledEvent;
    class RenderResources;
    class RenderViewport;

    struct TextureViewerData
    {
        bool m_IsRenderingNeeded = false;

        TextureId m_ReferenceTextureId = g_MaxEnum<TextureId>;
        bool m_IsReferenceTextureValid = false;

        uint32_t m_ActiveDepthIndex = 0;
        uint32_t m_ActiveMipIndex = 0;

        std::array<bool, 4> m_IsChannelActive;
        float m_MinColor = 0.0f;
        float m_MaxColor = 1.0f;
    };

    class TextureViewerTool : public ImGuiTool
    {
    public:
        TextureViewerTool(TextureViewerData& viewerData, RenderViewport& viewport, const RenderResources& resources);

    private:
        void OnEvent(Event& event) override;
        void DrawWindow() override;
        void DrawWindowContent() override;

        void DrawChannelCheckbox(const char* name, const ImVec4& textColor, const ImVec4& checkMarkColor, uint32_t channelIndex);
        void DrawShaderConsts(uint32_t textureDepth, uint32_t mipCount);
        void DrawDebugTexture() const;

        bool OnMouseMovedEvent(const MouseMovedEvent& event);
        bool OnMouseScrolledEvent(const MouseScrolledEvent& event);

    private:
        TextureViewerData& m_ViewerData;
        RenderViewport& m_Viewport;
        const RenderResources& m_Resources;

        bool m_IsFullViewportPreview = false;

        ImVec2 m_UvMin{ 0.0f, 0.0f };
        ImVec2 m_UvMax{ 1.0f, 1.0f };
    };

}
