#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class Backend;
    class Device;
    class GpuProfiler;
    class RenderViewport;
    class ShaderManager;

    class PerformanceOverlayTool : public ImGuiTool
    {
    public:
        PerformanceOverlayTool(
            const Backend& backend,
            const Device& device,
            const ShaderManager& shaderManager,
            const GpuProfiler& gpuProfiler,
            const RenderViewport& viewport,
            const TickTimer& frameTimer);

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

        const Backend& m_Backend;
        const Device& m_Device;
        const ShaderManager& m_ShaderManager;
        const GpuProfiler& m_GpuProfiler;
        const RenderViewport& m_Viewport;
        const TickTimer& m_FrameTimer;

        OverlayLocation m_Location = OverlayLocation::BottomLeft;

        float m_AvgFps = 0.0;
        float m_SmoothedCpuTimeInMs = 0.0f;
        float m_SmoothedFullCpuTimeInMs = 0.0f;
        float m_SmoothedGpuWaitTimeInMs = 0.0f;
        float m_SmoothedGpuTimeInMs = 0.0f;
    };

}
