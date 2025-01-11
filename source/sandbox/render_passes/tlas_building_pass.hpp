#include <benzin/engine/render_pass.hpp>

namespace benzin
{

    class Device;
    class RayTracingScene;

}

namespace sandbox
{

    class TlasBuildingPass : public benzin::RenderPass
    {
    public:
        TlasBuildingPass(benzin::Device& device, benzin::RayTracingScene& rayTracingScene);

        bool IsDependentOnViewport() const override { return false; }

        void OnRender() const override;

    private:
        benzin::Device& m_Device;
        benzin::RayTracingScene& m_RayTracingScene;
    };

}