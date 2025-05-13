#pragma once

#include <benzin/core/enum_flags.hpp>
#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/geometry_resources.hpp>

namespace benzin
{
    struct Mesh;

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
            AlphaTest,
        };

        struct DrawMeshInstance
        {
            entt::entity EntityHandle = benzin::g_BadEnum<entt::entity>;
            std::vector<uint32_t> MeshInstanceIndices;
        };

        void CreatePso(benzin::PsoId id, benzin::EnumFlags<PsoFlag> flags = {});
        void SetPso(benzin::GraphicsCmdList& cmdList, benzin::PsoId meshId, benzin::PsoId vertexId) const;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

        bool IsSphereCulled(const DirectX::BoundingSphere& localBoundingSphere, const DirectX::XMMATRIX& localToWorldMatrix) const;

        void GroupMeshInstances() const;
        void ProcessMesh(entt::entity entityHandle, const benzin::Mesh& mesh, const DirectX::XMMATRIX& localToWorldMatrix) const;
        void RenderMeshInstances(benzin::GraphicsCmdList& cmdList, std::span<const DrawMeshInstance> drawMeshes) const;

    private:
        bool m_IsDepthPrePassEnabled = true;
        bool m_IsCpuFrustumCullingEnabled = true;
        bool m_IsMeshPipelineUsed = true;

        joint::GeometryPassConsts m_Consts{};

        mutable std::vector<DrawMeshInstance> m_MeshInstances;
        mutable std::vector<DrawMeshInstance> m_AlphaMeshInstances;
    };

}
