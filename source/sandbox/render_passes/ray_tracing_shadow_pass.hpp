#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/ray_tracing_shadow_resources.hpp>

namespace sandbox
{

    class RayTracing_ShadowPass : public benzin::RenderPass
    {
    public:
        RayTracing_ShadowPass();
        ~RayTracing_ShadowPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        joint::RayTracing_ShadowConsts m_Consts{};

        std::unique_ptr<benzin::Texture> m_BlueNoiseTexture;
        uint16_t m_BlueNoiseDepthIndex = 0;
    };

}
