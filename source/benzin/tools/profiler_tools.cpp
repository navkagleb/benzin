#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/profiler_tools.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    static uint32_t SpawnEventRow(std::span<const ProfileEvent> events, uint32_t eventIndex, bool isOpen)
    {
        if (!isOpen)
        {
            return eventIndex + 1;
        }

        const auto& event = events[eventIndex++];
        BenzinAssert(event.Name != nullptr);

        ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_SpanFullWidth;
        treeFlags |= ImGuiTreeNodeFlags_SpanAllColumns;
        treeFlags |= ImGuiTreeNodeFlags_FramePadding;

        if (event.Depth == 0)
        {
            treeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        const std::string eventTime = std::format("{:.4f} ms", benzin::ToFloatMs(event.Us));

        if (!event.IsParent)
        {
            treeFlags |= ImGuiTreeNodeFlags_Leaf;
            treeFlags |= ImGuiTreeNodeFlags_NoTreePushOnOpen;

            ImGui::TreeNodeEx((void*)(intptr_t)eventIndex, treeFlags, event.Name);

            ImGui::TableNextColumn();
            ImGui::Text(eventTime.c_str());
        }
        else
        {
            isOpen = ImGui::TreeNodeEx((void*)(intptr_t)eventIndex, treeFlags, event.Name);

            ImGui::TableNextColumn();
            ImGui::Text(eventTime.c_str());

            while (eventIndex < events.size() && event.Depth < events[eventIndex].Depth)
            {
                eventIndex = SpawnEventRow(events, eventIndex, isOpen);
            }

            if (isOpen)
            {
                ImGui::TreePop();
            }
        }

        return eventIndex;
    }

    static void SpawnEventRows(std::span<const ProfileEvent> events)
    {
        for (uint32_t eventIndex = 0; eventIndex < events.size();)
        {
            eventIndex = SpawnEventRow(events, eventIndex, true);
        }
    }

    // ProfilerToolBase

    ProfilerToolBase::ProfilerToolBase(std::string_view name)
        : ImGuiTool{ name }
        , m_IntervalTimer{ std::chrono::milliseconds{ 1000 } }
    {
        m_IntervalTimer.PushCallback([this](uint32_t frameCount)
        {
            for (auto& event : m_SmoothEvents)
            {
                event.Us /= frameCount;
            }

            m_ReadySmoothEvents = std::move(m_SmoothEvents);
        });
    }

    void ProfilerToolBase::SpawnImGui()
    {
        const auto events = GetSortedEvents();

        if (m_SmoothEvents.size() != events.size())
        {
            m_SmoothEvents.resize(events.size());
            std::ranges::copy(events, m_SmoothEvents.begin());
        }
        else
        {
            for (uint32_t i = 0; i < events.size(); ++i)
            {
                m_SmoothEvents[i].Us += events[i].Us;
            }
        }

        m_IntervalTimer.AccumulateInterval(*ms_FrameTimer);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        BenzinExecuteOnScopeExit([] { ImGui::PopStyleVar(); });

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar;
        SpawnImGuiWindow(windowFlags, [this]
        {
            const ImGuiTableFlags tableFlags =
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_NoBordersInBody |
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_NoSavedSettings;

            if (ImGui::BeginTable("GpuProfilerData", 2, tableFlags, ImGui::GetContentRegionAvail()))
            {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Ms", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 10.0f);

                SpawnEventRows(m_ReadySmoothEvents);

                ImGui::EndTable();
            }
        });
    }

    // ProfilerTool

    ProfilerTool::ProfilerTool()
        : ProfilerToolBase{ "Profiler" }
    {}

    std::span<const ProfileEvent> ProfilerTool::GetSortedEvents() const
    {
        return Profiler::GetSortedEvents();
    }

    // GpuProfilerTool

    GpuProfilerTool::GpuProfilerTool(const GpuProfiler& gpuProfiler)
        : ProfilerToolBase{ "GpuProfiler" }
        , m_GpuProfiler{ gpuProfiler }
    {}

    std::span<const ProfileEvent> GpuProfilerTool::GetSortedEvents() const
    {
        return m_GpuProfiler.GetSortedEvents();
    }

}
