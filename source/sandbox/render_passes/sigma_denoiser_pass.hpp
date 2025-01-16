#pragma once

#include <benzin/graphics2/render_pass.hpp>


namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class Scene;

}

namespace joint
{

    struct SigmaConstants;

}

namespace sandbox
{

    class SigmaDenoiserPass : public benzin::RenderPass
    {
    public:
        static const benzin::GraphicsFormat s_PenumbraFormat;
        static const uint32_t s_MaxHistoryLength;

    public:
        explicit SigmaDenoiserPass(const benzin::Scene& scene);
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
        const benzin::Scene& m_Scene;

        using SigmaConstantBuffer = benzin::ConstantBuffer<joint::SigmaConstants>;
        std::unique_ptr<SigmaConstantBuffer> m_SigmaConstantBuffer;

        DirectX::XMUINT2 m_TileCount{};
    };

}
