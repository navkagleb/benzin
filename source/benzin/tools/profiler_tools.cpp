#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/profiler_tools.hpp>

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

    template <typename ProfileNodeT>
    static void DrawNodeRecursive(const ProfileNodeT& node, ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_None)
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

        if constexpr (std::is_same_v<ProfileNodeT, ProfileNode>)
        {
            ImGui::TableNextColumn();
            ImGui::FmtText("{}", node.m_ImGuiHitCount);
        }

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

            for (const auto& child : node.m_Children)
            {
                DrawNodeRecursive(*child);
            }

            ImGui::TreePop();
        }
    }

    // ProfilerToolBase

    template <typename ProfileNodeT>
    void ProfilerToolBase<ProfileNodeT>::DrawWindow()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGuiTool::DrawWindow(ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
    }

    template <typename ProfileNodeT>
    void ProfilerToolBase<ProfileNodeT>::DrawWindowContent()
    {
        BenzinProfile();

        constexpr ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_NoBordersInBody |
            ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_NoSavedSettings;

        constexpr bool isGpuProfilerTool = std::is_same_v<ProfileNodeT, GpuProfileNode>;

        if (ImGui::BeginTable("ProfilerData", isGpuProfilerTool ? 2 : 3, tableFlags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_NoResize);
            if constexpr (!isGpuProfilerTool)
            {
                ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 4.0f);
            }
            ImGui::TableSetupColumn("Ms", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("A").x * 10.0f);

            const ProfileNodeT* rootNode = GetRootNode();
            if (rootNode != nullptr)
            {
                DrawNodeRecursive(*rootNode, ImGuiTreeNodeFlags_DefaultOpen);
            }

            ImGui::EndTable();
        }
    }

    // ProfilerTool

    ProfilerTool::ProfilerTool()
        : ProfilerToolBase{ "Engine/Profiler" }
    {
        ms_IntervalTimer->AddCallback([](float, uint32_t frameCount)
        {
            Profiler::ResetAccumulatedData(frameCount);
        });
    }

    const ProfileNode* ProfilerTool::GetRootNode() const
    {
        return Profiler::GetRootNode();
    }

    // GpuProfilerTool

    GpuProfilerTool::GpuProfilerTool(GpuProfiler& gpuProfiler)
        : ProfilerToolBase{ "Graphics/GpuProfiler" }
        , m_GpuProfiler{ gpuProfiler }
    {
        ms_IntervalTimer->AddCallback([this](float, uint32_t frameCount)
        {
            m_GpuProfiler.ResetAccumulatedData(frameCount);
        });
    }

    const GpuProfileNode* GpuProfilerTool::GetRootNode() const
    {
        return m_GpuProfiler.GetRootNode();
    }

}
