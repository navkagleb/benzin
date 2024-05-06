#pragma once

#include "sandbox/runner.hpp"
#include "sandbox/tools/timings_tool.hpp"

namespace sandbox
{

    class RenderPassSettingsTool;

    enum class SandboxTiming : uint32_t
    {
        BuildTopLevelAs,
        GeometryPass,
        RtShadowPass,
        DenoiserPass,
        DenoiserPass_Accumulation,
        DenoiserPass_Mips,
        DenoiserPass_HistoryFix,
        DenoiserPass_Blur,
        DeferredLightingPass,
        EnvironmentPass,
        FullScreenDebugPass,
        ImGuiPass,
        BackBufferCopy,
        Total,
    };
    BenzinEnableUnaryPlusForEnum(SandboxTiming);

    enum SceneMesh
    {
        Sponza,
        BoomBox,
        DamagedHelmet,
        OrientationTest,
        Cylinder,
        Sphere,
    };
    BenzinEnableUnaryPlusForEnum(SceneMesh);

    class SandboxRunner : public Runner
    {
    private:
        using SceneMeshes = benzin::EnumArray<uint32_t, SceneMesh>;

        void Client_InitRenderPasses() override;
        void Client_InitTools() override;
        void Client_InitSceneEntities() override;

        void InitCamera();
        void InitSceneEntities();

        void Client_OnEvent(benzin::Event& event) override;
        void Client_AfterEndFrame() override;

        void LoadAndCreateMeshes(SceneMeshes& outSceneMeshes);
        void CreateEntities(const SceneMeshes& sceneMeshes);

    private:
        RenderPassSettingsTool* m_RenderPassSettingsTool = nullptr;
        TimingsTool<SandboxTiming, SandboxTiming>* m_TimingsTool = nullptr;

        bool m_IsAnimationEnabled = false;
        entt::entity m_PointLightEntity;
    };

}
