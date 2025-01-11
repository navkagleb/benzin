#pragma once

#include "sandbox/runner.hpp"
#include "sandbox/tools/timings_tool.hpp"

namespace benzin
{

    struct MeshCollectionResource;

}

namespace sandbox
{

    enum class RenderPasses : uint32_t
    {
        TlasBuilding,
        GlobalConstants,
        Geometry,
        RayTracedShadows,
        SigmaDenoiser,
        DeferredLighting,
        Environment,
        FullScreenDebug,
        ImGui,
        CopuToBackBuffer,
    };
    BenzinEnableUnaryPlusForEnum(RenderPasses);

    class SandboxRunner : public Runner
    {
    public:
        SandboxRunner();

    private:
        void InitRenderPasses();
        void InitTools();

        void InitSceneEntities();
        void InitCamera();

        void LoadMeshes(std::span<benzin::MeshCollectionResource> outMeshResources);
        void AddMeshesToScene(std::span<benzin::MeshCollectionResource> meshResources, std::span<entt::entity> outMeshHandles);
        void CreateEntities(std::span<const entt::entity> meshHandles);

    private:
        using TimingsTool = TimingsTool<RenderPasses, RenderPasses>;
        TimingsTool* m_TimingsTool = nullptr;

        Timings<RenderPasses> m_CpuTimings{};
    };

}
