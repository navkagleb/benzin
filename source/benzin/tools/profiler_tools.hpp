#pragma once

#include <benzin/core/profiler.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class GpuProfiler;

    struct ProfileEvent;

    template <typename ProfileNodeT>
    class ProfilerToolBase : public ImGuiTool
    {
    public:
        using ImGuiTool::ImGuiTool;

    protected:
        virtual const ProfileNodeT* GetRootNode() const = 0;

    private:
        void DrawWindow() override;
        void DrawWindowContent() override;
    };

    class ProfilerTool : public ProfilerToolBase<ProfileNode>
    {
    public:
        ProfilerTool();

    private:
        const ProfileNode* GetRootNode() const override;
    };

    class GpuProfilerTool : public ProfilerToolBase<GpuProfileNode>
    {
    public:
        explicit GpuProfilerTool(GpuProfiler& gpuProfiler);

    private:
        const GpuProfileNode* GetRootNode() const override;

        GpuProfiler& m_GpuProfiler;
    };

}
