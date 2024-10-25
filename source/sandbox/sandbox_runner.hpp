#pragma once

#include "sandbox/runner.hpp"
#include "sandbox/tools/timings_tool.hpp"

namespace sandbox
{

    enum class RenderPasses : uint32_t
    {
        GlobalConstants,
        Geometry,
        RayTracingShadows,
        SigmaDenoiser,
        DeferredLighting,
        Environment,
        FullScreenDebug,
        ImGui,
        CopuToBackBuffer,
    };
    BenzinEnableUnaryPlusForEnum(RenderPasses);

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
        using TimingsTool = TimingsTool<RenderPasses, RenderPasses>;

        void InitRenderPasses();
        void InitTools();

        void InitSceneEntities();
        void InitCamera();

        void LoadAndCreateMeshes(SceneMeshes& outSceneMeshes);
        void CreateEntities(const SceneMeshes& sceneMeshes);

    private:
        TimingsTool* m_TimingsTool = nullptr;

        Timings<RenderPasses> m_CpuTimings{};
    };

}
