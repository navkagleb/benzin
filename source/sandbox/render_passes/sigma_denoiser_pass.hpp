#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/sigma_denoiser_resources.hpp>

namespace sandbox
{

    class SigmaDenoiserPass : public benzin::RenderPass
    {
    public:
        SigmaDenoiserPass();
        ~SigmaDenoiserPass() override;

    private:
        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

        void RunClearPass(bool isEnabled) const;
        void RunClassifyTilesPass(uint16_t sliceIndex) const;
        void RunSmoothTilesPass() const;
        void RunBlurPass(uint16_t sliceIndex) const;
        void RunPostBlurPass(bool isEnabled) const;
        void RunTemporalStabilizationPass(bool isEnabled, uint16_t sliceIndex) const;

    private:
        joint::SigmaConsts m_Consts{};
        std::vector<joint::SigmaPerLightConsts> m_PerLightConsts;
    };

}
