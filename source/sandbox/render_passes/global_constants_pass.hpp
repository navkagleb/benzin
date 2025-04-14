#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/global_resources.hpp>

namespace benzin
{
    class Scene;
}

namespace sandbox
{

    class GlobalConstantsPass : public benzin::RenderPass
    {
    public:
        explicit GlobalConstantsPass(const benzin::Scene& scene);

    private:
        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate() override;
        void OnRender() const override;

        void UpdateCameraConsts();
        void UpdateFrameConsts();

    private:
        const benzin::Scene& m_Scene;

        joint::FrameConsts m_FrameConsts{};

        DirectX::XMUINT2 m_PrevRenderResolution{ 0, 0 };
        float m_PrevAnimationElapsedTimeInSec = 0.0f;
    };

}
