#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace sandbox
{

    class CopyToBackBufferPass : public benzin::RenderPass
    {
    public:
        bool IsDependentOnViewport() const override { return false; }

        void OnRender() const override;
    };

}
