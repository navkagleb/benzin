#pragma once

#include <benzin/engine/render_pass.hpp>

#include <shaders/joint/constant_buffer_types.hpp>

namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class Device;
    class Scene;

}

namespace sandbox
{

    class GlobalConstantsPass : public benzin::RenderPass
    {
    public:
        GlobalConstantsPass(benzin::Device& device, benzin::Scene& scene);
        ~GlobalConstantsPass() override;

        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate() override;
        void OnRender() const override;

    private:
        void UpdateCameraConstants();

    private:
        benzin::Device& m_Device;
        benzin::Scene& m_Scene;

        using FrameConstantBuffer = benzin::ConstantBuffer<joint::FrameConstants>;
        std::unique_ptr<FrameConstantBuffer> m_FrameConstantBuffer;

        joint::CameraConstants m_CameraConstants{};
        joint::CameraConstants m_PrevCameraConstants{};

        DirectX::XMUINT2 m_PrevRenderResolution{ 0, 0 };
    };

}
