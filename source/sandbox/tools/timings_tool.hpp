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

        explicit TimingsTool(const benzin::Device& device, bool isVisible = false)
            : ImGuiTool{ "TimingsTool", isVisible }
            , m_Device{ device }
        {}

        void SetCpuTimings(const CpuTimings& timings) { m_CpuTimings = timings; }

    private:
        void OnImGuiRender() override
        {
            for (const auto& [i, timing] : m_GpuTimings | std::views::enumerate)
            {
                timing = m_Device.GetGpuTimer().GetElapsedTime((uint32_t)i);
            }

            RenderImGuiWindow([this]
            {
                RenderImGuiTimings<CpuTimingT>("CpuTimings", m_CpuTimings);
                RenderImGuiTimings<GpuTimingT>("GpuTimings", m_GpuTimings);
            });
        }

        template <benzin::EnumConcept TimingT>
        void RenderImGuiTimings(std::string_view name, std::span<const std::chrono::microseconds> timings)
        {
            const auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected;
            if (ImGui::TreeNodeEx(name.data(), flags))
            {
                for (const auto [i, timing] : timings | std::views::enumerate)
                {
                    const uint32_t indent = GetTimingIndent((TimingT)i);
                    ImGui::Text(BenzinFormatCstr("{:{}}{}: {:.4f} ms", "", indent, magic_enum::enum_name((TimingT)i), benzin::ToFloatMs(timing)));
                }

                ImGui::TreePop();
            }
        }

    private:
        const benzin::Device& m_Device;

        CpuTimings m_CpuTimings{};
        GpuTimings m_GpuTimings{};
    };

}
