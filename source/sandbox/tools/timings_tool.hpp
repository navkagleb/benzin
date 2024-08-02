#pragma once

#include <benzin/engine/imgui_pass.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    class Device;

}

namespace sandbox
{

    template <benzin::EnumConcept TimingT>
    using Timings = benzin::EnumArray<std::chrono::microseconds, TimingT>;

    enum class RunnerTiming : uint32_t
    {
        BeginFrame,
        OnUpdate,
        OnRender,
        EndFrame,
    };
    BenzinEnableUnaryPlusForEnum(RunnerTiming);

    using RunnerTimings = Timings<RunnerTiming>;

    template <benzin::EnumConcept TimingT>
    uint32_t GetTimingIndent(TimingT timing)
    {
        BenzinUnused(timing);
        return 0;
    }

    template <benzin::EnumConcept CpuTimingT, benzin::EnumConcept GpuTimingT>
    class TimingsTool : public benzin::ImGuiTool
    {
    public:
        using CpuTimings = Timings<CpuTimingT>;
        using GpuTimings = Timings<GpuTimingT>;

        explicit TimingsTool(const benzin::Device& device)
            : ImGuiTool{ "TimingsTool", true }
            , m_Device{ device }
        {}

        void SetRunnerTimings(const RunnerTimings& timings) { m_RunnerTimings = timings; }
        void SetCpuTimings(const CpuTimings& timings) { m_CpuTimings = timings; }

    private:
        void SpawnImGui() override
        {
            for (const auto& [i, timing] : m_GpuTimings | std::views::enumerate)
            {
                timing = m_Device.GetGpuTimer().GetElapsedTime((uint32_t)i);
            }

            SpawnImGuiWindow([this]
            {
                SpawnImGuiTimings<RunnerTiming>("RunnerTimings", m_RunnerTimings);
                SpawnImGuiTimings<CpuTimingT>("CpuTimings", m_CpuTimings);
                SpawnImGuiTimings<GpuTimingT>("GpuTimings", m_GpuTimings);
            });
        }

        template <benzin::EnumConcept TimingT>
        void SpawnImGuiTimings(std::string_view name, std::span<const std::chrono::microseconds> timings) const
        {
            if (SpawnImGuiCollapsingHeader(name))
            {
                for (const auto [i, timing] : timings | std::views::enumerate)
                {
                    const uint32_t indent = GetTimingIndent((TimingT)i);
                    ImGui::Text(BenzinFormatData("{:{}}{}: {:.4f} ms", "", indent, magic_enum::enum_name((TimingT)i), benzin::ToFloatMs(timing)));
                }
            }
        }

    private:
        const benzin::Device& m_Device;

        RunnerTimings m_RunnerTimings{};
        CpuTimings m_CpuTimings{};
        GpuTimings m_GpuTimings{};
    };

}
