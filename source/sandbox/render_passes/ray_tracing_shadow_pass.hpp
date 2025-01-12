#pragma once

#include <benzin/engine/render_pass.hpp>

namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class Buffer;
    class PipelineState;
    class Scene;
    class Texture;
    class RayTracing_ShaderTable;

}

namespace joint
{

    struct RayTracing_ShadowConsts;

}

namespace sandbox
{

    class RayTracing_ShadowPass : public benzin::RenderPass
    {
    public:
        explicit RayTracing_ShadowPass(const benzin::Scene& scene);
        ~RayTracing_ShadowPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnRenderViewportResize() override;

        void OnUpdate() override;
        void OnRender() const override;

    private:
        void BuildShaderTable();

    private:
        const benzin::Scene& m_Scene;

        benzin::PipelineState* m_Pso = nullptr;
        std::unique_ptr<benzin::RayTracing_ShaderTable> m_ShaderTable;
        std::unique_ptr<benzin::Buffer> m_TableBuffer;

        using PassConstantBuffer = benzin::ConstantBuffer<joint::RayTracing_ShadowConsts>;
        std::unique_ptr<PassConstantBuffer> m_PassConstBuffer;

        std::unique_ptr<benzin::Texture> m_BlueNoise;
    };

}
