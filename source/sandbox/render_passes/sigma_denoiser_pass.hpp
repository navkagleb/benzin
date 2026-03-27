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

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        void RunClearPass(bool isEnabled) const;
        void RunClassifyTilesPass() const;
        void RunSmoothTilesPass() const;
        void RunBlurPass() const;
        void RunPostBlurPass(bool isEnabled) const;
        void RunTemporalStabilizationPass(bool isEnabled) const;

        joint::SigmaConsts m_Consts = {};
    };

}
