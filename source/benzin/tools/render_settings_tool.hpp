#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class RenderSettingsTool : public ImGuiTool
    {
    public:
        template <typename T>
        using SettingsDrawCallback = std::function<void(T& settings)>;

        RenderSettingsTool(RenderSettings& renderSettings);

        template <typename T>
        void RegisterSectionDrawCallback(const SettingsDrawCallback<T>& drawCallback, ImGuiTreeNodeFlags flags)
        {
            m_SectionInfos.push_back(SectionInfo
            {
                .TitleName = GetClassName<T>(),
                .Flags = flags,
                .DrawCallback = [this, drawCallback]
                {
                    drawCallback(m_RenderSettings.GetSection<T>());
                },
            });
        }

    private:
        void DrawWindowContent() override;

    private:
        struct SectionInfo
        {
            std::string_view TitleName;
            ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None;
            ImGui_DrawCallback DrawCallback;
        };

        RenderSettings& m_RenderSettings;

        std::vector<SectionInfo> m_SectionInfos;
    };

}
