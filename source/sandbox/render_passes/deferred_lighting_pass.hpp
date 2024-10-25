#pragma once

#include <benzin/engine/render_pass.hpp>

#include <shaders/joint/constant_buffer_types.hpp>

namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class PipelineState;
    class Scene;

}

namespace sandbox
{

    class DeferredLightingPass : public benzin::RenderPass
    {
    public:
        explicit DeferredLightingPass(const benzin::Scene& scene);
        ~DeferredLightingPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;

        void OnUpdate() override;
        void OnRender() const override;

    private:
        const benzin::Scene& m_Scene;

        benzin::PipelineState* m_Pso = nullptr;

        using PassConstantBuffer = benzin::ConstantBuffer<joint::DeferredLightingPassConstants>;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

}
