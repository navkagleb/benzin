#pragma once

#include "benzin/engine/imgui_pass.hpp"

namespace benzin
{

    class Backend;
    class Device;
    class RenderViewportTool;
    class SwapChain;
    class Window;

    class PerformanceOverlayTool : public ImGuiTool
    {
    public:
        PerformanceOverlayTool(
            const Window& window,
            const Device& device,
            const SwapChain& swapChain,
            const RenderViewportTool& renderViewportTool
        );

        void SetFrameRateStats(float frameRate, float dt);

    private:
        void SpawnImGui() override;

    private:
        const Window& m_Window;
        const Device& m_Device;
        const SwapChain& m_SwapChain;
        const RenderViewportTool& m_RenderViewportTool;

        float m_FrameRate = 0.0f;
        float m_FrameDeltaTimeMs = 0.0f;
    };

}
