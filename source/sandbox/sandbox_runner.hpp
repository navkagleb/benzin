#pragma once

#include "sandbox/runner.hpp"

namespace benzin
{
    struct MeshResource;
}

namespace sandbox
{

    class SandboxRunner : public Runner
    {
    public:
        ~SandboxRunner() override;

        void InitRenderPasses() override;
        void InitTools() override;
    };

    class SponzaRunner : public SandboxRunner
    {
    private:
        void InitScene() override;

        void InitSceneEntities();
        void InitCamera();

        void AddMeshesToScene(std::span<benzin::MeshResource> meshResources, std::span<entt::entity> outMeshHandles);

        void AddStaticMeshEntities(std::span<const entt::entity> meshHandles);
        void AddDynamicMeshEntities(std::span<const entt::entity> meshHandles);
        void AddProceduralGrass();
        void AddLightEntities(std::span<const entt::entity> meshHandles);
    };

    class StanfordDragonRunner : public SandboxRunner
    {
    private:
        void InitScene() override;
    };

}
