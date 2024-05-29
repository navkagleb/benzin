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
    public:
        SandboxRunner();

    private:
        using SceneMeshes = benzin::EnumArray<uint32_t, SceneMesh>;
        using TimingsTool = TimingsTool<SandboxTiming, SandboxTiming>;

        void InitRenderPasses();
        void InitTools();

        void InitSceneEntities();
        void InitCamera();

        void LoadAndCreateMeshes(SceneMeshes& outSceneMeshes);
        void CreateEntities(const SceneMeshes& sceneMeshes);

    private:
        RenderPassSettingsTool* m_RenderPassSettingsTool = nullptr;
        TimingsTool* m_TimingsTool = nullptr;
    };

}
