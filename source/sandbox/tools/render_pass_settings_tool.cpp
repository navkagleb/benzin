#include "sandbox/bootstrap.hpp"
#include "sandbox/tools/render_pass_settings_tool.hpp"

#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/scene.hpp>

namespace sandbox
{

    static void RenderImGuiSection(std::string_view titleName, bool isLast, const std::function<void()>& renderCallback)
    {
        static constexpr ImVec4 titleColor{ 0.72f, 39.0f, 0.0f, 1.0f };

        ImGui::TextColored(titleColor, titleName.data());

        renderCallback();

        if (!isLast)
        {
            ImGui::Separator();
        }
    }

    //

    RenderPassSettingsTool::RenderPassSettingsTool(benzin::Scene& scene, RenderPassSettings& settings)
        : ImGuiTool{ "RenderPassSettingsTool", true }
        , m_Scene{ scene }
        , m_Settings{ settings }
    {}

    void RenderPassSettingsTool::OnImGuiRender()
    {
        RenderImGuiWindow([this]
        {
            static constexpr ImVec4 titleColor{ 0.72f, 39.0f, 0.0f, 1.0f };

            if (m_PointLightEntity != benzin::g_InvalidEnumValue<entt::entity>)
            {
                RenderImGuiSection("PointLight", false, [this]
                {
                    if (auto* plc = m_Scene.GetEntityRegistry().try_get<benzin::PointLightComponent>(m_PointLightEntity))
                    {
                        ImGui::SliderFloat("GeometryRadius", &plc->GeometryRadius, 0.00001f, 0.15f);
                    }
                });
            }

            RenderImGuiSection("RtShadows", false, [this]
            {
                ImGui::Checkbox("IsRtShadowsEnabled", &m_Settings.IsRtShadowEnabled);
                ImGui::SliderInt("RaysPerPixel", (int*)&m_Settings.RaysPerPixel, 0, 100);
            });

            RenderImGuiSection("Denoiser", false, [this]
            {
                ImGui::Checkbox("IsDenoiserEnabled", &m_Settings.IsDenoiserEnabled);
                ImGui::Checkbox("IsGeometryWeightUsed", &m_Settings.IsGeometryWeightUsed);
                ImGui::Checkbox("IsNormalWeightUsed", &m_Settings.IsNormalWeightUsed);
                ImGui::Checkbox("IsRoughnessWeightUsed", &m_Settings.IsRoughnessWeightUsed);
                ImGui::DragFloat("GeometryWeightSensitivity", &m_Settings.GeometryWeightSensitivity, 0.2f, 1.0f, 50.0f);
                ImGui::DragInt("MaxTemporalAccumulationCount", (int*)&m_Settings.MaxTemporalAccumulationCount, 0.2f, 1, 64);
            });

            RenderImGuiSection("DeferredLightingParams", false, [this]
            {
                ImGui::DragFloat("SunIntensity", &m_Settings.SunIntensity, 0.1f, 0.0f, 100.0f);
                ImGui::ColorEdit3("SunColor", reinterpret_cast<float*>(&m_Settings.SunColor));

                if (ImGui::DragFloat3("SunDirection", reinterpret_cast<float*>(&m_Settings.SunDirection), 0.01f, -1.0f, 1.0f))
                {
                    DirectX::XMStoreFloat3(&m_Settings.SunDirection, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_Settings.SunDirection)));
                }
            });

            RenderImGuiSection("FullScreenDebugParams", true, [this]
            {
                ImGui::SliderInt("ViewDepthMipIndex", (int*)&m_Settings.ViewDepthMipIndex, 0, 4);
                ImGui::SliderFloat("MinViewDepth", &m_Settings.MinViewDepth, 0.001f, 2.0f, "%.4f");
                ImGui::SliderFloat("MaxViewDepth", &m_Settings.MaxViewDepth, 0.001f, 30.0f);

                ImGui::Text("DebugOutputType");
                if (ImGui::BeginListBox("##emtpy", ImVec2{ -FLT_MIN, 230.0f })) // #TODO: Calculate item height
                {
                    for (const auto i : std::views::iota(0u, magic_enum::enum_count<joint::DebugOutputType>()))
                    {
                        const bool isSelected = m_Settings.DebugOutputType == i;

                        const auto name = magic_enum::enum_name((joint::DebugOutputType)i).substr("DebugOutputType_"sv.size());
                        if (ImGui::Selectable(name.data(), isSelected))
                        {
                            m_Settings.DebugOutputType = (joint::DebugOutputType)i;
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndListBox();
                }
            });
        });
    }

}
