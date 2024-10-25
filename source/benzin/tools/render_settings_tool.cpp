#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/render_settings_tool.hpp"

namespace benzin
{

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
                if (SpawnImGuiCollapsingHeader(sectionInfo.TitleName, sectionInfo.IsOpenByDefault))
                {
                    sectionInfo.ImGuiSpawnCallback();
                }
            }
        });
    }

}
