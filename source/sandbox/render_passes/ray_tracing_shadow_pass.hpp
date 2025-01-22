#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/ray_tracing_shadow_resources.hpp>

namespace benzin
{

    class Scene;
    class Texture;

}

namespace sandbox
{

    class RayTracing_ShadowPass : public benzin::RenderPass
    {
    public:
        explicit RayTracing_ShadowPass(const benzin::Scene& scene);
        ~RayTracing_ShadowPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        const benzin::Scene& m_Scene;

        joint::RayTracing_ShadowConsts m_Consts{};
        std::unique_ptr<benzin::Texture> m_BlueNoise;
    };

}
