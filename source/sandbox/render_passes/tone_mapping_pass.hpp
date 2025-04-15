#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/tone_mapping_resources.hpp>

namespace benzin
{
    class ComputeCmdList;
}

namespace sandbox
{

    class ToneMappingPass : public benzin::RenderPass
    {
    public:
        ToneMappingPass();
        ~ToneMappingPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        void RunClearPass(benzin::ComputeCmdList& cmdList) const;
        void RunCalcLuminanceHistogramPass(benzin::ComputeCmdList& cmdList) const;
        void RunCalcAvgLuminancePass(benzin::ComputeCmdList& cmdList) const;
        void RunApplyToneMapOperatorPass(benzin::ComputeCmdList& cmdList) const;

    private:
        joint::ToneMappingConsts m_Consts{};
    };

}
