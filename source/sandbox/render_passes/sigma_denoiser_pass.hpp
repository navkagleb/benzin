#pragma once

#include <benzin/engine/render_pass.hpp>

#include <shaders/joint/sigma_denoiser_resources.hpp>

namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class PipelineState;
    class Scene;

}

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

    private:
        benzin::PipelineState* m_ClassifyTilesPso = nullptr;
        benzin::PipelineState* m_SmoothTilesPso = nullptr;
        benzin::PipelineState* m_BlurPso = nullptr;
        benzin::PipelineState* m_PostBlurPso = nullptr;
        benzin::PipelineState* m_TemporalStabilizationPso = nullptr;

        using SigmaConstantBuffer = benzin::ConstantBuffer<joint::SigmaConstants>;
        std::unique_ptr<SigmaConstantBuffer> m_SigmaConstantBuffer;

        DirectX::XMUINT2 m_TileCount{};
    };

}
