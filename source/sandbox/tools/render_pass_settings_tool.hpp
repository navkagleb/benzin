#pragma once

#include <benzin/engine/imgui_pass.hpp>

#include <shaders/joint/enum_types.hpp>

namespace benzin
{

    class Scene;

}

namespace sandbox
{

    struct RenderPassSettings
    {
        float SunIntensity = 0.0f;
        DirectX::XMFLOAT3 SunColor{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 SunDirection{ -0.5f, -0.5f, -0.5f };

        bool IsRtShadowEnabled = true;
        uint32_t RaysPerPixel = 1;

        bool IsDenoiserEnabled = true;
        bool IsGeometryWeightUsed = true;
        bool IsNormalWeightUsed = true;
        bool IsRoughnessWeightUsed = true;
        float GeometryWeightSensitivity = 20.0f;
        float MinBlurRadius = 0.01f;
        float MaxBlurRadius = 0.2f;
        uint32_t MaxTemporalAccumulationCount = 32;

        joint::DebugOutputType DebugOutputType = joint::DebugOutputType_None;
        uint32_t ViewDepthMipIndex = 0;
        float MinViewDepth = 0.0f;
        float MaxViewDepth = 20.0f;
    };

    class RenderPassSettingsTool : public benzin::ImGuiTool
    {
    public:
        RenderPassSettingsTool(benzin::Scene& scene, RenderPassSettings& settings);

        void SetPointLightEntity(entt::entity entity) { m_PointLightEntity = entity; }

    private:
        void OnImGuiRender() override;

    private:
        benzin::Scene& m_Scene;
        RenderPassSettings& m_Settings;
        entt::entity m_PointLightEntity = benzin::g_InvalidEnumValue<entt::entity>;
    };

}
