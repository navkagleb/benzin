#pragma once

#include <benzin/engine/render_pass.hpp>

namespace benzin
{

    class Scene;
    class PipelineState;

    struct MeshCollection;

}

namespace sandbox
{

    class GeometryPass : public benzin::RenderPass
    {
    public:
        explicit GeometryPass(const benzin::Scene& scene);
        ~GeometryPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;

        void OnRender() const override;

    private:
        bool IsMeshCulled(
            const benzin::MeshCollection& meshCollection,
            uint32_t meshInstanceIndex,
            const DirectX::XMMATRIX& localToWorldMatrix
        ) const;

    private:
        const benzin::Scene& m_Scene;

        benzin::PipelineState* m_Pso = nullptr;
    };

}
