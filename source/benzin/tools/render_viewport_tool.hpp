#pragma once

#include "benzin/engine/imgui_pass.hpp"
#include "benzin/engine/camera.hpp"

namespace benzin
{

    class RenderViewportTool : public ImGuiTool
    {
    public:
        friend class FlyCameraController;

        RenderViewportTool(RenderResources& renderResources, Camera& camera);

        auto& GetFlyCameraController() { return m_FlyCameraController; }

        uint32_t GetWidth() const { return (uint32_t)m_ViewportSize.x; }
        uint32_t GetHeight() const { return (uint32_t)m_ViewportSize.y; }

        bool IsViewportSizeRelevant() const { return m_IsViewportSizeRelevant; }
        bool IsValidForRendering() const { return m_IsViewportSizeRelevant && m_IsVisible; }

        void SetFinalTextureIndex(uint32_t finalTextureIndex) { m_FinalTextureIndex = finalTextureIndex; }

    private:
        void OnEvent(Event& event) override;
        void SpawnImGui() override;

        void UpdateImGuiDimensions();

    private:
        RenderResources& m_RenderResources;
        FlyCameraController m_FlyCameraController;

        uint32_t m_FinalTextureIndex = g_InvalidUnsigned<uint32_t>;
        
        DirectX::XMINT2 m_ViewportSize{};
        bool m_IsViewportSizeRelevant = true;

        bool m_IsViewportHovered = false;
        bool m_IsViewportActive = false;
    };

}
