#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/full_screen_debug_resources.hpp>

namespace sandbox
{

    class FullScreenDebugPass : public benzin::RenderPass
    {
    public:
        FullScreenDebugPass();
        ~FullScreenDebugPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnUpdate() override;
        void OnRender() const override;

    private:
        joint::FullScreenDebugConsts m_Consts;
    };

}
