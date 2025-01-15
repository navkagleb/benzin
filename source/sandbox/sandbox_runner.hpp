#pragma once

#include "sandbox/runner.hpp"
#include "sandbox/tools/timings_tool.hpp"

namespace benzin
{

    struct MeshResource;

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

        void AddMeshesToScene(std::span<benzin::MeshResource> meshResources, std::span<entt::entity> outMeshHandles);

        void AddStaticMeshEntities(std::span<const entt::entity> meshHandles);
        void AddDynamicMeshEntities(std::span<const entt::entity> meshHandles);
        void AddEmissiveEntities(std::span<const entt::entity> meshHandles);


    private:
        using TimingsTool = TimingsTool<RenderPasses, RenderPasses>;
        TimingsTool* m_TimingsTool = nullptr;

        Timings<RenderPasses> m_CpuTimings{};
    };

}
