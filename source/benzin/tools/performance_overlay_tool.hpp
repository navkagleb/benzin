#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class Device;
    class RenderViewportTool;
    class ShaderManager;
    class Window;

    class PerformanceOverlayTool : public ImGuiTool
    {
    public:
        PerformanceOverlayTool(
            const Window& window,
            const Device& device,
            const ShaderManager& shaderManager,
            const RenderViewportTool& renderViewportTool
        );

        void SetFrameRateStats(float frameRate, float dt);

    private:
        void SpawnImGui() override;

    private:
        const Window& m_Window;
        const Device& m_Device;
        const ShaderManager& m_ShaderManager;
        const RenderViewportTool& m_RenderViewportTool;

        float m_FrameRate = 0.0f;
        float m_FrameDeltaTimeMs = 0.0f;
    };

}
