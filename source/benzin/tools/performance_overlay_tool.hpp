#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class Backend;
    class Device;
    class RenderViewport;
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
            const RenderViewport& viewport
        );

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
        const RenderViewport& m_Viewport;

        OverlayLocation m_Location = OverlayLocation::BottomLeft;

        float m_AvgFps = 0.0;
        float m_AvgDeltaTimeInMs = 0.0f;
    };

}
