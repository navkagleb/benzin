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
    private:
        void InitRenderPasses() override;
        void InitTools() override;
        void InitScene() override;

        void InitSceneEntities();
        void InitCamera();

        void AddMeshesToScene(std::span<benzin::MeshResource> meshResources, std::span<entt::entity> outMeshHandles);

        void AddStaticMeshEntities(std::span<const entt::entity> meshHandles);
        void AddDynamicMeshEntities(std::span<const entt::entity> meshHandles);
        void AddProceduralGrass();
        void AddLightEntities(std::span<const entt::entity> meshHandles);
    };

}
