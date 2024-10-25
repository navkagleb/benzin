#pragma once

#include <benzin/engine/render_pass.hpp>

#include <shaders/joint/constant_buffer_types.hpp>

namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class PipelineState;
    class Scene;
    class Buffer;

}

namespace sandbox
{

    class RayTracingShadowsPass : public benzin::RenderPass
    {
    public:
        explicit RayTracingShadowsPass(const benzin::Scene& scene);
        ~RayTracingShadowsPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;

        void OnUpdate() override;
        void OnRender() const override;

    private:
        void CreatePipelineStateObject();
        void CreateShaderTable();

    private:
        const benzin::Scene& m_Scene;

        ComPtr<ID3D12StateObject> m_D3D12RaytracingStateObject;

        std::unique_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::unique_ptr<benzin::Buffer> m_MissShaderTable;
        std::unique_ptr<benzin::Buffer> m_HitGroupShaderTable;

        using PassConstantBuffer = benzin::ConstantBuffer<joint::RayTracingShadowsConstants>;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

}
