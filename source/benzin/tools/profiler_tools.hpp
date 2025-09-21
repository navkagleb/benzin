#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class GpuProfiler;

    struct ProfileEvent;

    class ProfilerToolBase : public ImGuiTool
    {
    public:
        using ImGuiTool::ImGuiTool;

    private:
        void DrawWindow() override;
    };

    class ProfilerTool : public ProfilerToolBase
    {
    public:
        ProfilerTool();

    private:
        void DrawWindowContent() override;
    };

    class GpuProfilerTool : public ProfilerToolBase
    {
    public:
        explicit GpuProfilerTool(const GpuProfiler& gpuProfiler);

    private:
        void DrawWindowContent() override;

        const GpuProfiler& m_GpuProfiler;

        std::vector<ProfileEvent> m_SmoothEvents;
        std::vector<ProfileEvent> m_ReadySmoothEvents;
    };

}
