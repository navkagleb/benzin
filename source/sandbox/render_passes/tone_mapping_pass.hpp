#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/tone_mapping_resources.hpp>

namespace benzin
{
    class GraphicsCommandList;
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
        void RunClearPass(benzin::GraphicsCommandList& cmdList) const;
        void RunCalcLuminanceHistogramPass(benzin::GraphicsCommandList& cmdList) const;
        void RunCalcAvgLuminancePass(benzin::GraphicsCommandList& cmdList) const;
        void RunApplyToneMapOperatorPass(benzin::GraphicsCommandList& cmdList) const;

    private:
        joint::ToneMappingConsts m_Consts{};
    };

}
