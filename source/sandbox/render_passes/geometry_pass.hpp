#pragma once

#include <benzin/engine/render_pass.hpp>

namespace benzin
{

    class Scene;

}

namespace sandbox
{

    class GeometryPass : public benzin::RenderPass
    {
    public:
        explicit GeometryPass(const benzin::Scene& scene);
        ~GeometryPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;

        void OnRender() const override;

    private:
        const benzin::Scene& m_Scene;
    };

}
