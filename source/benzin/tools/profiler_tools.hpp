#pragma once

#include <benzin/graphics2/imgui_pass.hpp>

namespace benzin
{

    class GpuProfiler;

    struct ProfileEvent;

    class ProfilerToolBase : public ImGuiTool
    {
    public:
        explicit ProfilerToolBase(std::string_view name);

        virtual std::span<const ProfileEvent> GetSortedEvents() const = 0;

    private:
        void DrawWindow() override;
        void DrawWindowContent() override;

    private:
        std::vector<ProfileEvent> m_SmoothEvents;
        std::vector<ProfileEvent> m_ReadySmoothEvents;
    };

    class ProfilerTool : public ProfilerToolBase
    {
    public:
        ProfilerTool();

    private:
        std::span<const ProfileEvent> GetSortedEvents() const override;
    };

    class GpuProfilerTool : public ProfilerToolBase
    {
    public:
        explicit GpuProfilerTool(const GpuProfiler& gpuProfiler);

    private:
        std::span<const ProfileEvent> GetSortedEvents() const override;

    private:
        const GpuProfiler& m_GpuProfiler;
    };

}
