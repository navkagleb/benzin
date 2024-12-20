#pragma once

#include <benzin/engine/render_pass.hpp>

#include <shaders/joint/sigma_denoiser_resources.hpp>

namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class PipelineState;

}

namespace sandbox
{

    class SigmaDenoiserPass : public benzin::RenderPass
    {
    public:
        static const benzin::GraphicsFormat s_PenumbraFormat;
        static const uint32_t s_MaxHistoryLength;

    public:
        SigmaDenoiserPass();
        ~SigmaDenoiserPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;

        void OnUpdate(const benzin::TickTimer& tickTimer) override;
        void OnRender() const override;

    private:
        void RunClearPass(bool isEnabled) const;
        void RunClassifyTilesPass() const;
        void RunSmoothTilesPass() const;
        void RunBlurPass() const;
        void RunPostBlurPass(bool isEnabled) const;
        void RunTemporalStabilizationPass(bool isEnabled) const;

    private:
        enum class Step
        {
            ClassifyTiles,
            SmoothTiles,
            CopyHistory,
            Blur,
            PostBlur,
            TemporalStabilization,
        };

        std::array<benzin::PipelineState*, magic_enum::enum_count<Step>()> m_Psos{};

        using SigmaConstantBuffer = benzin::ConstantBuffer<joint::SigmaConstants>;
        std::unique_ptr<SigmaConstantBuffer> m_SigmaConstantBuffer;

        DirectX::XMUINT2 m_TileCount{};
    };

}
