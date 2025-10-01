#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/gpu_info_tool.hpp>

#include <benzin/graphics/adl_wrapper.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/nvapi_wrapper.hpp>

namespace benzin
{

    static void DrawColoredBulletText(const char* name, float mb)
    {
        const ImVec4 orangeColor{ 1.0f, 0.5f, 0.0f, 1.0f };

        ImGui::BulletText(name);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, orangeColor);
        ImGui::FmtText("{:.2f}", mb);
        ImGui::PopStyleColor();
    }

    static void DrawGpuInfo(const AdapterInfo& info, const AdapterMemoryInfo& memoryInfo)
    {
        {
            ImGui::SeparatorText("General info");

            ImGui::FmtBulletText("GPU Cores: {}", info.m_GpuCoreCount);
        }

        {
            ImGui::SeparatorText("Local VRAM (in MB)");

            DrawColoredBulletText("Used by process:", ToMb(memoryInfo.m_UsedLocalVramInBytes));
            DrawColoredBulletText("Available in system (vendor specific):", ToMb(memoryInfo.m_AvailableVramInBytes));

            ImGui::BeginDisabled();
            ImGui::FmtBulletText("Total: {:.2f}", ToMb(info.m_TotalLocalVramInBytes));
            ImGui::FmtBulletText("OS Budget: {:.2f}", ToMb(memoryInfo.m_LocalVramBudgetInBytes));
            ImGui::FmtBulletText("Available relative to OS Budget (vendor specific): {:.2f}", ToMb(memoryInfo.m_AvailableVramRelativeToOsBudgetInBytes));
            ImGui::EndDisabled();
        }

        {
            ImGui::SeparatorText("Host VRAM (in MB)");

            DrawColoredBulletText("Used by process:", ToMb(memoryInfo.m_UsedHostVramInBytes));

            ImGui::BeginDisabled();
            ImGui::FmtBulletText("Total: {:.2f}", ToMb(info.m_TotalHostVramInBytes));
            ImGui::FmtBulletText("OS Budget: {:.2f}", ToMb(memoryInfo.m_HostVramBudgetInBytes));
            ImGui::EndDisabled();
        }
    }

    //

    GpuInfoTool::GpuInfoTool(const Backend& backend)
        : ImGuiTool{ "Graphics/GpuInfo" }
        , m_Backend{ backend }
    {}

    void GpuInfoTool::DrawWindowContent()
    {
        const uint32_t adapterCount = m_Backend.GetAdapterCount();
        for (uint32_t i = 0; i < adapterCount; ++i)
        {
            const AdapterInfo& info = m_Backend.GetAdapterInfo(i);
            const AdapterMemoryInfo memoryInfo = m_Backend.GetAdapterMemoryInfo(i);

            ImGui::CollapsingHeaderWithIndent(
                info.m_Name,
                [&info, &memoryInfo] { DrawGpuInfo(info, memoryInfo); },
                m_Backend.GetMainAdapterIndex() == i ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None
            );
        }
    }

}
