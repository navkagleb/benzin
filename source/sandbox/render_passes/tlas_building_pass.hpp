#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace sandbox
{

    class TlasBuildingPass : public benzin::RenderPass
    {
    public:
        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate() override;
        void OnRender() const override;
    };

}