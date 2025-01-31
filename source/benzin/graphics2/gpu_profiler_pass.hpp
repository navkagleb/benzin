#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{

    class GpuProfilerPass : public RenderPass
    {
    private:
        bool IsDependentOnViewport() const { return false; }

        void OnRender() const override;
    };

}
