#pragma once

#include "benzin/engine/imgui_pass.hpp"

namespace benzin
{

    class RenderViewportTool : public ImGuiTool
    {
    public:
        explicit RenderViewportTool(RenderResources& renderResources);

        uint32_t GetWidth() const { return (uint32_t)m_ViewportSize.x; }
        uint32_t GetHeight() const { return (uint32_t)m_ViewportSize.y; }

        bool IsViewportResized() const { return m_IsViewportResized; }

        void SetFinalTextureKey(uint32_t finalTextureKey) { m_FinalTextureKey = finalTextureKey; }

    private:
        void OnEvent(Event& event) override;
        void SpawnImGui() override;

        void UpdateImGuiDimensions();

    private:
        RenderResources& m_RenderResources;

        uint32_t m_FinalTextureKey = g_InvalidUnsigned<uint32_t>;
        
        DirectX::XMINT2 m_ViewportSize{};
        bool m_IsViewportResized = false;

        bool m_IsViewportHovered = false;
    };

}
