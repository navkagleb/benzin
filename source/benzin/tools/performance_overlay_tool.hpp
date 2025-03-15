#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class Backend;
    class Device;
    class RenderViewportTool;
    class ShaderManager;
    class Window;

    class PerformanceOverlayTool : public ImGuiTool
    {
    public:
        PerformanceOverlayTool(
            const Window& window,
            const Backend& backend,
            const Device& device,
            const ShaderManager& shaderManager,
            const RenderViewportTool& renderViewportTool
        );

        void SetFrameRateStats(float frameRate, float dt);

    private:
        void DrawWindow() override;
        void DrawWindowContent() override;

    private:
        enum class OverlayLocation : int8_t
        {
            Custom = -1,
            TopLeft = 0,
            TopRight,
            BottomLeft,
            BottomRight,
        };

        const Window& m_Window;
        const Backend& m_Backend;
        const Device& m_Device;
        const ShaderManager& m_ShaderManager;
        const RenderViewportTool& m_RenderViewportTool;

        float m_FrameRate = 0.0f;
        float m_FrameDeltaTimeMs = 0.0f;

        OverlayLocation m_Location = OverlayLocation::BottomLeft;
    };

}
