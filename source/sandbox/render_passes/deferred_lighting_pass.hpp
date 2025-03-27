#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace sandbox
{

    class DeferredLightingPass : public benzin::RenderPass
    {
    public:
        DeferredLightingPass();
        ~DeferredLightingPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnRender() const override;
    };

}
