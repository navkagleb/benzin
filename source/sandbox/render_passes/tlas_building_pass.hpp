#include <benzin/engine/render_pass.hpp>

namespace benzin
{

    class Device;
    class RayTracing_Scene;

}

namespace sandbox
{

    class TlasBuildingPass : public benzin::RenderPass
    {
    public:
        TlasBuildingPass(benzin::Device& device, benzin::RayTracing_Scene& rayTracingScene);

        bool IsDependentOnViewport() const override { return false; }

        void OnRender() const override;

    private:
        benzin::Device& m_Device;
        benzin::RayTracing_Scene& m_RayTracingScene;
    };

}