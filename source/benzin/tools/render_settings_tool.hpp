#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class RenderSettingsTool : public ImGuiTool
    {
    public:
        template <typename T>
        using SettingsSpawnCallback = std::function<void(T& settings)>;

        RenderSettingsTool(RenderSettings& renderSettings);

        template <typename T>
        void RegisterSectionSpawnCallback(std::string_view name, bool isOpenByDefault, const SettingsSpawnCallback<T>& spawnCallback)
        {
            m_SectionInfos.push_back(SectionInfo
            {
                .TitleName = name,
                .IsOpenByDefault = isOpenByDefault,
                .SpawnCallback = [this, spawnCallback]
                {
                    spawnCallback(m_RenderSettings.GetSection<T>());
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
            std::function<void()> SpawnCallback;
        };

        RenderSettings& m_RenderSettings;

        std::vector<SectionInfo> m_SectionInfos;
    };

}
