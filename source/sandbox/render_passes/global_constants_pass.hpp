#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/global_resources.hpp>

namespace benzin
{

    class Device;
    class Scene;

}

namespace sandbox
{

    class GlobalConstantsPass : public benzin::RenderPass
    {
    public:
        GlobalConstantsPass(benzin::Device& device, const benzin::Scene& scene);
        ~GlobalConstantsPass() override;

        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate(const benzin::TickTimer& frameTimer) override;
        void OnRender() const override;

    private:
        void UpdateCameraConsts();
        void UpdateFrameConsts(const benzin::TickTimer& frameTimer);

    private:
        benzin::Device& m_Device;
        const benzin::Scene& m_Scene;

        joint::FrameConsts m_FrameConsts{};

        DirectX::XMUINT2 m_PrevRenderResolution{ 0, 0 };
    };

}
