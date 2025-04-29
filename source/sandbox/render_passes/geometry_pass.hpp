#pragma once

#include <benzin/core/enum_flags.hpp>
#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{
    class GraphicsCmdList;
    class MeshInstanceComponent;

    enum class PsoId : uint32_t;
}

namespace sandbox
{

    struct GBufferStats;
    struct GBufferSettings;

    class GeometryPass : public benzin::RenderPass
    {
    public:
        GeometryPass();
        ~GeometryPass() override;

    private:
        enum class PsoFlag
        {
            Mesh,
            DepthPrePass,
            IndexOrderClockwise,
        };

        void CreatePso(benzin::PsoId id, benzin::EnumFlags<PsoFlag> flags = {});
        void SetPso(benzin::GraphicsCmdList& cmdList, benzin::PsoId meshId, benzin::PsoId vertexId) const;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

        bool IsSphereCulled(
            const DirectX::BoundingSphere& localBoundingSphere,
            const DirectX::XMMATRIX& localToWorldMatrix,
            const DirectX::XMMATRIX& localInstanceMatrix = DirectX::XMMatrixIdentity()
        ) const;

        void RenderMeshes(benzin::GraphicsCmdList& cmdList, bool isIndexOrderClockwise) const;
        void RenderLights(benzin::GraphicsCmdList& cmdList) const;

        void RenderMesh(benzin::GraphicsCmdList& cmdList, const benzin::MeshInstanceComponent& meshInstanceComponent, const DirectX::XMMATRIX& localToWorldMatrix) const;

    private:
        bool m_IsDepthPrePassEnabled = true;
        bool m_IsCpuFrustumCullingEnabled = true;
        bool m_IsMeshPipelineUsed = true;
    };

}
