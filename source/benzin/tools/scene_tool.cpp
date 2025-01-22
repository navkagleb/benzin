#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/scene_tool.hpp"

#include "benzin/engine/scene.hpp"
#include "benzin/engine/light.hpp"

namespace benzin
{

    SceneTool::SceneTool(Scene& scene)
        : ImGuiTool{ "SceneTool" }
        , m_Scene{ scene }
    {}

    void SceneTool::SpawnImGui()
    {
        SpawnImGuiWindow([this]
        {
            SpawnSun();
            SpawnSphericalLights();
        });
    }

    void SceneTool::SpawnSun()
    {
        if (!SpawnImGuiCollapsingHeader("Sun"))
        {
            return;
        }

        auto& sun = m_Scene.GetEntityRegistry().get<SunLight>(m_Scene.GetSunEntity());

        ImGui::PushID(magic_enum::enum_integer(m_Scene.GetSunEntity()));
        BenzinExecuteOnScopeExit([] { ImGui::PopID(); });

        auto* color = (DirectX::XMFLOAT3*)&sun.GetColor();
        ImGui::ColorEdit3("Color", (float*)color);

        float intensity = sun.GetIntensity();
        if (ImGui::InputFloat("Intensity", &intensity))
        {
            sun.SetIntensity(intensity);
        }

        float angularDiameter = sun.GetAngularDiameterInRadians();
        if (ImGui::SliderAngle("AngularDiameter", &angularDiameter, 0.0f, 3.0f, "%.3f"))
        {
            sun.SetAngularDiameterInRadians(angularDiameter);
        }

        float azimuth = sun.GetAzimuthInRadians();
        if (ImGui::SliderAngle("Azimuth", &azimuth, 0.0f, 360.0f))
        {
            sun.SetAzimuthInRadians(azimuth);
        }

        float elevation = sun.GetElevationInRadians();
        if (ImGui::SliderAngle("Elevation", &elevation, 0.0f, 180.0f))
        {
            sun.SetElevationInRadians(elevation);
        }
    }

    void SceneTool::SpawnSphericalLights()
    {
        if (!SpawnImGuiCollapsingHeader("SphericalLights"))
        {
            return;
        }

        const auto view = m_Scene.GetEntityRegistry().view<SphericalLight>();
        for (const entt::entity entity : view)
        {
            ImGui::PushID(magic_enum::enum_integer(entity));
            BenzinExecuteOnScopeExit([] { ImGui::PopID(); });

            auto& light = view.get<SphericalLight>(entity);

            const auto headerName = std::format("Light: {}", magic_enum::enum_integer(entity));
            if (!ImGui::CollapsingHeader(headerName.c_str()))
            {
                continue;
            }

            bool isEnabled = light.IsEnabled();
            if (ImGui::Checkbox("IsEnabled", &isEnabled))
            {
                light.SetEnabled(isEnabled);
            }

            auto* color = (DirectX::XMFLOAT3*)&light.GetColor();
            ImGui::ColorEdit3("Color", (float*)color);

            float intensity = light.GetIntensity();
            if (ImGui::InputFloat("Intensity", &intensity))
            {
                light.SetIntensity(intensity);
            }

            auto* position = (float*)&light.GetPosition();
            if (ImGui::InputFloat3("Position", position))
            {
                light.SetPosition(*(DirectX::XMFLOAT3*)position);
            }

            float range = light.GetRange();
            if (ImGui::InputFloat("Range", &range))
            {
                light.SetRange(range);
            }

            const auto& attenuation = light.GetAttenuation();
            ImGui::Text(BenzinFormatData("Attenuation: {} {} {}", attenuation.x, attenuation.y, attenuation.z));

            float radius = light.GetRadius();
            if (ImGui::InputFloat("Radius", &radius))
            {
                light.SetRadius(radius);
            }
        }

    }

}
