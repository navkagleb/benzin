#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/global_resources.hpp>

namespace benzin
{
    class GraphicsCmdList;
}

namespace sandbox
{

    class GlobalConstsPass : public benzin::RenderPass
    {
    public:
        using ReadbackStatsCallback = std::function<void(std::span<const uint32_t> readbackStats)>;

        GlobalConstsPass(ReadbackStatsCallback&& callback);
        ~GlobalConstsPass() override;

    private:
        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate() override;
        void OnRender() const override;

        void UpdateCameraConsts();
        void UpdateFrameConsts();

        void CopyStats(benzin::GraphicsCmdList& cmdList) const;

    private:
        joint::FrameConsts m_FrameConsts{};

        DirectX::XMUINT2 m_PrevRenderResolution{ 0, 0 };
        float m_PrevAnimationElapsedTimeInSec = 0.0f;

        ReadbackStatsCallback m_ReadbackStatsCallback;
    };

}
