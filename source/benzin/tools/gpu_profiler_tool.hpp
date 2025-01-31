#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class GpuProfiler;

    class GpuProfilerTool : public ImGuiTool
    {
    public:
        explicit GpuProfilerTool(const GpuProfiler& gpuProfiler);

    private:
        void SpawnImGui() override;

    private:
        const GpuProfiler& m_GpuProfiler;
    };

}
