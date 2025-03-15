#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/gpu_info_tool.hpp>

#include <benzin/graphics/backend.hpp>

namespace benzin
{

    static void DrawColoredBulletText(const char* name, float mb)
    {
        const ImVec4 orangeColor{ 1.0f, 0.5f, 0.0f, 1.0f };

        ImGui::BulletText(name);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, orangeColor);
        ImGui::Text(BenzinFormatData("{:.2f}", mb));
        ImGui::PopStyleColor();
    }

    static void DrawGpuInfo(const AdapterInfo& info, const AdapterMemoryInfo& memoryInfo)
    {
        {
            ImGui::SeparatorText("VRAM (in MB)");

            DrawColoredBulletText("Used by process:", memoryInfo.ProcessUsedVram.GetMb());
            DrawColoredBulletText("Available:", memoryInfo.AvailableVram.GetMb());

            ImGui::BeginDisabled();
            ImGui::BulletText(BenzinFormatData("Total: {:.2f}", info.TotalVram.GetMb()));
            ImGui::BulletText(BenzinFormatData("OS Budget: {:.2f}", memoryInfo.VramOsBudget.GetMb()));
            ImGui::BulletText(BenzinFormatData("Available relative to OS Budget: {:.2f}", memoryInfo.AvailableVramRelativeToOsBudget.GetMb()));
            ImGui::EndDisabled();
        }

        {
            ImGui::SeparatorText("Shared RAM (in MB)");

            DrawColoredBulletText("Used by process:", memoryInfo.ProcessUsedSharedRam.GetMb());

            ImGui::BeginDisabled();
            ImGui::BulletText(BenzinFormatData("Total: {:.2f}", info.TotalSharedRam.GetMb()));
            ImGui::BulletText(BenzinFormatData("OS Budget: {:.2f}", memoryInfo.SharedRamOsBudget.GetMb()));
            ImGui::EndDisabled();
        }
    }

    //

    GpuInfoTool::GpuInfoTool(const Backend& backend)
        : ImGuiTool{ "GpuInfo" }
        , m_Backend{ backend }
    {}

    void GpuInfoTool::DrawWindowContent()
    {
        const uint32_t adapterCount = m_Backend.GetAdapterCount();

        for (uint32_t i = 0; i < adapterCount; ++i)
        {
            const AdapterInfo& info = m_Backend.GetAdapterInfo(i);
            const AdapterMemoryInfo memoryInfo = m_Backend.GetAdapterMemoryInfo(i);

            ImGui_CollapsingHeaderWithIndent(
                info.Name,
                [&info, &memoryInfo] { DrawGpuInfo(info, memoryInfo); },
                m_Backend.GetMainAdapterIndex() == i ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None
            );
        }
    }


}
