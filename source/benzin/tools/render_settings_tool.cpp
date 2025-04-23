#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/render_settings_tool.hpp"

namespace benzin
{

    RenderSettingsTool::RenderSettingsTool(RenderSettings& renderSettings)
        : ImGuiTool{ "RenderSettingsTool" }
        , m_RenderSettings{ renderSettings }
    {}

    void RenderSettingsTool::DrawWindowContent()
    {
        for (const SectionInfo& sectionInfo : m_SectionInfos)
        {
            if (ImGui_MainCollapsingHeader(sectionInfo.TitleName, sectionInfo.Flags))
            {
                BenzinAssert(sectionInfo.DrawCallback);
                sectionInfo.DrawCallback();
            }
        }
    }

}
