#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/sigma_denoiser_resources.hpp>

namespace benzin
{

    class Scene;

}

namespace sandbox
{

    class SigmaDenoiserPass : public benzin::RenderPass
    {
    public:
        explicit SigmaDenoiserPass(const benzin::Scene& scene);
        ~SigmaDenoiserPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        void RunClearPass(bool isEnabled) const;
        void RunClassifyTilesPass(uint16_t sliceIndex) const;
        void RunSmoothTilesPass() const;
        void RunBlurPass(uint16_t sliceIndex) const;
        void RunPostBlurPass(bool isEnabled) const;
        void RunTemporalStabilizationPass(bool isEnabled, uint16_t sliceIndex) const;

    private:
        const benzin::Scene& m_Scene;

        joint::SigmaConsts m_Consts{};
        std::vector<joint::SigmaPerLightConsts> m_PerLightConsts;
    };

}
