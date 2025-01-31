#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/gpu_profiler_tool.hpp>

#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    static uint32_t SpawnEvent(std::span<const GpuProfiler::Event> events, uint32_t eventIndex, bool isImGuiSpawned)
    {
        if (!isImGuiSpawned)
        {
            return eventIndex + 1;
        }

        ImGuiTreeNodeFlags flags = eventIndex == 0 ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
        flags |= ImGuiTreeNodeFlags_FramePadding;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        const auto& event = events[eventIndex++];
        const auto eventString = std::format("{}: {:.4f} ms", event.GetName(), benzin::ToFloatMs(event.GetUs()));

        if (!event.IsParent())
        {
            flags |= ImGuiTreeNodeFlags_Leaf;

            if (ImGui::TreeNodeEx((void*)(intptr_t)eventIndex, flags, eventString.c_str()))
            {
                ImGui::TreePop();
            }
        }
        else
        {
            isImGuiSpawned = ImGui::TreeNodeEx((void*)(intptr_t)eventIndex, flags, eventString.c_str());

            while (eventIndex < events.size() && event.GetDepth() < events[eventIndex].GetDepth())
            {
                eventIndex = SpawnEvent(events, eventIndex, isImGuiSpawned);
            }

            if (isImGuiSpawned)
            {
                ImGui::TreePop();
            }
        }

        return eventIndex;
    }

    GpuProfilerTool::GpuProfilerTool(const GpuProfiler& gpuProfiler)
        : ImGuiTool{ "GpuProfilerTool" }
        , m_GpuProfiler{ gpuProfiler }
    {}

    void GpuProfilerTool::SpawnImGui()
    {
        SpawnImGuiWindow([this]
        {
            const auto events = m_GpuProfiler.GetSortedEvents();
            SpawnEvent(events, 0, !events.empty());
        });
    }

}
