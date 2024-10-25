#pragma once

#include "benzin/engine/imgui_pass.hpp"

namespace benzin
{

    class RenderSettingsTool : public ImGuiTool
    {
    public:
        RenderSettingsTool(RenderSettings& renderSettings);

        template <typename T>
        void RegisterSectionImGuiSpawnCallback(std::string_view name, bool isOpenByDefault, const std::function<void(T&)>& imGuiSpawnCallback)
        {
            m_SectionInfos.push_back(SectionInfo
            {
                .TitleName = name,
                .IsOpenByDefault = isOpenByDefault,
                .ImGuiSpawnCallback = [this, imGuiSpawnCallback]
                {
                    imGuiSpawnCallback(m_RenderSettings.GetSection<T>());
                },
            });
        }

    private:
        void SpawnImGui() override;

    private:
        struct SectionInfo
        {
            std::string_view TitleName;
            bool IsOpenByDefault = false;
            std::function<void()> ImGuiSpawnCallback;
        };

        RenderSettings& m_RenderSettings;

        std::vector<SectionInfo> m_SectionInfos;
    };

}
