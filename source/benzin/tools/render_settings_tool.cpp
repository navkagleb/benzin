#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/render_settings_tool.hpp"

namespace benzin
{

    static void SpawnImGuiSection(std::string_view titleName, const std::function<void()>& imGuiRenderCallback)
    {
        static constexpr ImVec4 titleColor{ 0.72f, 39.0f, 0.0f, 1.0f };

        ImGui::TextColored(titleColor, titleName.data());
        imGuiRenderCallback();
        ImGui::Separator();
    }

    //

    RenderSettingsTool::RenderSettingsTool(RenderSettings& renderSettings)
        : ImGuiTool{ "RenderSettingsTool", true }
        , m_RenderSettings{ renderSettings }
    {}

    void RenderSettingsTool::SpawnImGui()
    {
        SpawnImGuiWindow([this]
        {
            for (const auto& sectionInfo : m_SectionInfos)
            {
                SpawnImGuiSection(sectionInfo.TitleName, sectionInfo.ImGuiSpawnCallback);
            }
        });
    }

}
