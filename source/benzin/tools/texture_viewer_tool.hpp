#pragma once

#include "benzin/engine/imgui_pass.hpp"

namespace benzin
{

    class RenderResources;

    class TextureViewerTool : public ImGuiTool
    {
    public:
        explicit TextureViewerTool(const RenderResources& renderResources);

        void SetTextureIndex(uint32_t textureIndex) { m_TextureIndex = textureIndex; }

    private:
        void OnEvent(Event& event) override;
        void SpawnImGui() override;

        void ClampUvs();

    private:
        const RenderResources& m_RenderResouces;

        uint32_t m_TextureIndex = g_InvalidUnsigned<uint32_t>;

        ImVec2 m_UvMin{ 0.0f, 0.0f };
        ImVec2 m_UvMax{ 1.0f, 1.0f };
        ImVec4 m_TintColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        bool m_IsHovered = false;
    };

}
