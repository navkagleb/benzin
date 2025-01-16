#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class RenderResources;

    class TextureViewerTool : public ImGuiTool
    {
    public:
        using TextureSelectorCallback = std::function<uint32_t()>;

        explicit TextureViewerTool(const RenderResources& renderResources);

        void SetTextureSelectorCallback(TextureSelectorCallback&& callback) { m_SelectorCallback = std::move(callback); }

    private:
        void OnEvent(Event& event) override;
        void SpawnImGui() override;

        void ClampUvs();

    private:
        const RenderResources& m_RenderResources;

        TextureSelectorCallback m_SelectorCallback;

        ImVec2 m_UvMin{ 0.0f, 0.0f };
        ImVec2 m_UvMax{ 1.0f, 1.0f };
        ImVec4 m_TintColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        bool m_IsHovered = false;
    };

}
