#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/profiler_tools.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    static void DrawRightAlignedText(const char* text)
    {
        const float availWidth = ImGui::GetContentRegionAvail().x;
        const float textWidth = ImGui::CalcTextSize(text).x;
        const float padding = ImGui::GetStyle().ItemSpacing.x;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availWidth - textWidth - padding);
        ImGui::TextUnformatted(text);
    }

    static uint32_t DrawEventRow(std::span<const ProfileEvent> events, uint32_t eventIndex, bool isOpen)
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

        const std::string eventTime = std::format("{:.3f} ms", benzin::ToFloatMs(event.Us));

        if (!event.IsParent)
        {
            treeFlags |= ImGuiTreeNodeFlags_Leaf;
            treeFlags |= ImGuiTreeNodeFlags_NoTreePushOnOpen;

            ImGui::TreeNodeEx((void*)(intptr_t)eventIndex, treeFlags, event.Name);

            ImGui::TableNextColumn();
            DrawRightAlignedText(eventTime.c_str());
        }
        else
        {
            isOpen = ImGui::TreeNodeEx((void*)(intptr_t)eventIndex, treeFlags, event.Name);

            ImGui::TableNextColumn();
            DrawRightAlignedText(eventTime.c_str());

            while (eventIndex < events.size() && event.Depth < events[eventIndex].Depth)
            {
                eventIndex = DrawEventRow(events, eventIndex, isOpen);
            }

            if (isOpen)
            {
                ImGui::TreePop();
            }
        }

        return eventIndex;
    }

    static void DrawEventRows(std::span<const ProfileEvent> events)
    {
        for (uint32_t eventIndex = 0; eventIndex < events.size();)
        {
            eventIndex = DrawEventRow(events, eventIndex, true);
        }
    }

    static void DrawNodeRecursive(const ProfileNode& node, ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_None)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        treeFlags |= ImGuiTreeNodeFlags_SpanFullWidth;
        treeFlags |= ImGuiTreeNodeFlags_SpanAllColumns;
        treeFlags |= ImGuiTreeNodeFlags_FramePadding;

        if (node.m_Children.empty())
        {
            treeFlags |= ImGuiTreeNodeFlags_Leaf;
        }

        const bool isOpen = ImGui::TreeNodeEx(&node, treeFlags, node.m_Name.data());

        ImGui::TableNextColumn();
        ImGui::FmtText("{}", node.m_ImGuiHitCount);

        const float durationInMs = std::chrono::duration<float, std::milli>{ node.m_ImGuiDuration }.count();
        const std::string durationStr = std::format("{:.3f} ms", durationInMs);

        ImGui::TableNextColumn();
        DrawRightAlignedText(durationStr.c_str());

        if (isOpen)
        {
            if (node.m_IsSortingNeeded)
            {
                node.SortChildren();
            }

            for (const std::unique_ptr<ProfileNode>& child : node.m_Children)
            {
                DrawNodeRecursive(*child);
            }

            ImGui::TreePop();
        }
    }

    // ProfilerToolBase

    void ProfilerToolBase::DrawWindow()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGuiTool::DrawWindow(ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
    }

    // ProfilerTool

    ProfilerTool::ProfilerTool()
        : ProfilerToolBase{ "Engine/Profiler" }
    {
        ms_IntervalTimer->AddCallback([this](float, uint32_t frameCount)
        {
            Profiler::ResetAccumulatedData(frameCount);
        });
    }

    void ProfilerTool::DrawWindowContent()
    {
        BenzinProfile();

        constexpr ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_NoBordersInBody |
            ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_NoSavedSettings;
        
        if (ImGui::BeginTable("ProfilerData", 3, tableFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 4.0f);
            ImGui::TableSetupColumn("Ms", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 10.0f);
            ImGui::TableHeadersRow();
        
            const ProfileNode* rootNode = &Profiler::GetRootNode();
            if (!rootNode->m_Children.empty())
            {
                rootNode = rootNode->m_Children.front().get();
            }

            DrawNodeRecursive(*rootNode, ImGuiTreeNodeFlags_DefaultOpen);
        
            ImGui::EndTable();
        }
    }

    // GpuProfilerTool

    GpuProfilerTool::GpuProfilerTool(const GpuProfiler& gpuProfiler)
        : ProfilerToolBase{ "Graphics/GpuProfiler" }
        , m_GpuProfiler{ gpuProfiler }
    {
        ms_IntervalTimer->AddCallback([this](float, uint32_t frameCount)
        {
            for (auto& event : m_SmoothEvents)
            {
                event.Us /= frameCount;
            }

            m_ReadySmoothEvents = std::move(m_SmoothEvents);
        });
    }

    void GpuProfilerTool::DrawWindowContent()
    {
        const auto events = m_GpuProfiler.GetSortedEvents();

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

        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_NoBordersInBody |
            ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_NoSavedSettings;

        if (ImGui::BeginTable("ProfilerData", 2, tableFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Ms", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 10.0f);

            DrawEventRows(m_ReadySmoothEvents);

            ImGui::EndTable();
        }
    }

}
