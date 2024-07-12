#include "sandbox/bootstrap.hpp"
#include "sandbox/tools/render_pass_settings_tool.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>

namespace sandbox
{

    static void RenderImGuiSection(std::string_view titleName, const std::function<void()>& renderCallback)
    {
        static constexpr ImVec4 titleColor{ 0.72f, 39.0f, 0.0f, 1.0f };

        ImGui::TextColored(titleColor, titleName.data());

        renderCallback();

        ImGui::Separator();
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
                RenderImGuiSection("PointLight", [this]
                {
                    auto& entityRegistry = m_Scene.GetEntityRegistry();

                    auto* plc = entityRegistry.try_get<benzin::PointLightComponent>(m_PointLightEntity);
                    BenzinEnsure(plc != nullptr);

                    if (ImGui::SliderFloat("GeometryRadius", &plc->GeometryRadius, 0.005f, 0.2f))
                    {
                        auto* tc = entityRegistry.try_get<benzin::TransformComponent>(m_PointLightEntity);
                        BenzinEnsure(tc);

                        tc->SetScale({ plc->GeometryRadius, plc->GeometryRadius, plc->GeometryRadius });
                    }
                });
            }

            RenderImGuiSection("RtShadows", [this]
            {
                ImGui::Checkbox("IsRtShadowsEnabled", &m_Settings.IsRtShadowEnabled);
                ImGui::SliderInt("RaysPerPixel", (int*)&m_Settings.RaysPerPixel, 0, 100);
            });

            RenderImGuiSection("Denoiser", [this]
            {
                ImGui::Checkbox("IsDenoiserEnabled", &m_Settings.IsDenoiserEnabled);
                ImGui::Checkbox("IsGeometryWeightUsed", &m_Settings.IsGeometryWeightUsed);
                ImGui::Checkbox("IsNormalWeightUsed", &m_Settings.IsNormalWeightUsed);
                ImGui::Checkbox("IsRoughnessWeightUsed", &m_Settings.IsRoughnessWeightUsed);
                ImGui::DragFloat("GeometryWeightSensitivity", &m_Settings.GeometryWeightSensitivity, 0.2f, 1.0f, 50.0f);
                ImGui::DragFloat("MinBlurRadius", &m_Settings.MinBlurRadius, 0.001f, 0.001f, 0.5f);
                ImGui::DragFloat("MaxBlurRadius", &m_Settings.MaxBlurRadius, 0.001f, 0.001f, 0.5f);
                ImGui::DragInt("MaxTemporalAccumulationCount", (int*)&m_Settings.MaxTemporalAccumulationCount, 0.2f, 1, 64);
            });

            RenderImGuiSection("DeferredLightingParams", [this]
            {
                ImGui::DragFloat("SunIntensity", &m_Settings.SunIntensity, 0.1f, 0.0f, 100.0f);
                ImGui::ColorEdit3("SunColor", reinterpret_cast<float*>(&m_Settings.SunColor));

                if (ImGui::DragFloat3("SunDirection", reinterpret_cast<float*>(&m_Settings.SunDirection), 0.01f, -1.0f, 1.0f))
                {
                    DirectX::XMStoreFloat3(&m_Settings.SunDirection, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_Settings.SunDirection)));
                }
            });

            RenderImGuiSection("FullScreenDebugParams", [this]
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
