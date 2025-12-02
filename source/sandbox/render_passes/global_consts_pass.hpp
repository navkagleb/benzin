#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/global_resources.hpp>

namespace benzin
{
    class Buffer;
}

namespace sandbox
{

    class GlobalConstsPass : public benzin::RenderPass
    {
    public:
        using ReadbackStatsCallback = std::function<void(std::span<const uint32_t> readbackStats)>;

        GlobalConstsPass(ReadbackStatsCallback&& callback);
        ~GlobalConstsPass() override;

        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate() override;
        void OnRender() const override;

    private:
        joint::FrameConsts m_FrameConsts = {};

        DirectX::XMUINT2 m_PrevRenderResolution{ 0, 0 };
        float m_PrevAnimationElapsedTimeInSec = 0.0f;

        std::unique_ptr<benzin::Buffer> m_StatBuffer;
        std::unique_ptr<benzin::Buffer> m_ReadbackStatBuffer;
        ReadbackStatsCallback m_ReadbackStatsCallback;
    };

}
