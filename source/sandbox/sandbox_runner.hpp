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
        SandboxRunner();

    private:
        void InitRenderPasses();
        void InitTools();

        void InitSceneEntities();
        void InitCamera();

        void AddMeshesToScene(std::span<benzin::MeshResource> meshResources, std::span<entt::entity> outMeshHandles);

        void AddStaticMeshEntities(std::span<const entt::entity> meshHandles);
        void AddDynamicMeshEntities(std::span<const entt::entity> meshHandles);
        void AddLightEntities(std::span<const entt::entity> meshHandles);
    };

}
