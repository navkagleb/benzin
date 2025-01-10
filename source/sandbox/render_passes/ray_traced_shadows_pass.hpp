#pragma once

#include <benzin/engine/render_pass.hpp>

#include <shaders/joint/ray_traced_shadows_resources.hpp>

namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class Buffer;
    class PipelineState;
    class Scene;
    class Texture;

}

namespace sandbox
{

    class RayTracedShadowsPass : public benzin::RenderPass
    {
    public:
        explicit RayTracedShadowsPass(const benzin::Scene& scene);
        ~RayTracedShadowsPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnRenderViewportResize() override;

        void OnUpdate() override;
        void OnRender() const override;

    private:
        void CreateShaderTable();

    private:
        const benzin::Scene& m_Scene;

        benzin::PipelineState* m_Pso = nullptr;
        std::unique_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::unique_ptr<benzin::Buffer> m_MissShaderTable;
        std::unique_ptr<benzin::Buffer> m_HitGroupShaderTable;

        using PassConstantBuffer = benzin::ConstantBuffer<joint::RayTracedShadowsConsts>;
        std::unique_ptr<PassConstantBuffer> m_PassConstBuffer;

        std::unique_ptr<benzin::Texture> m_BlueNoise;
    };

}
