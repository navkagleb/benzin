#include <benzin/engine/render_pass.hpp>

namespace benzin
{

    class Device;
    class Scene;

}

namespace sandbox
{

    class TlasBuildingPass : public benzin::RenderPass
    {
    public:
        TlasBuildingPass(benzin::Device& device, benzin::Scene& scene);

        bool IsDependentOnViewport() const override { return false; }

        void OnRender() const override;

    private:
        benzin::Device& m_Device;
        benzin::Scene& m_Scene;
    };

}