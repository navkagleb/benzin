#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{
    class RayTracing_Scene;
}

namespace sandbox
{

    class TlasBuildingPass : public benzin::RenderPass
    {
    public:
        explicit TlasBuildingPass(benzin::RayTracing_Scene& rayTracingScene);

    private:
        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate() override;
        void OnRender() const override;

    private:
        benzin::RayTracing_Scene& m_RayTracingScene;
    };

}