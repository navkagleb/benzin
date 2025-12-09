#pragma once

#include <benzin/graphics2/render_pass.hpp>
#include <shaders/joint/ray_tracing_shadow_resources.hpp>

namespace sandbox
{

    class RayTracingShadowPass : public benzin::RenderPass
    {
    public:
        RayTracingShadowPass();
        ~RayTracingShadowPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        joint::RayTracingShadowConsts m_Consts = {};
        std::unique_ptr<benzin::Texture> m_BlueNoiseTexture;
    };

}
