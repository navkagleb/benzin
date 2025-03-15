#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class Scene;
    class RayTracing_Scene;

    class SceneStatsTool : public ImGuiTool
    {
    public:
        SceneStatsTool(const Scene& scene, const RayTracing_Scene& rayTracingScene);

    private:
        void SpawnImGui() override;

        void SpawnSceneStats() const;
        void SpawnRayTracingSceneStats() const;

    private:
        const Scene& m_Scene;
        const RayTracing_Scene& m_RayTracingScene;
    };

}
