#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{
    struct MeshComponent;

    class Scene;
    class GraphicsCommandList;

    enum class IndexOrder : bool;
    enum class PsoId : uint32_t;
}

namespace sandbox
{

    struct GBufferStats;
    struct GBufferSettings;

    class GeometryPass : public benzin::RenderPass
    {
    public:
        explicit GeometryPass(const benzin::Scene& scene);
        ~GeometryPass() override;

    private:
        struct MeshRenderContext
        {
            benzin::GraphicsCommandList& CmdList;

            const DirectX::XMMATRIX& WorldToViewMatrix;
            const DirectX::BoundingFrustum& CameraFrustum;

            const entt::registry& EntityRegistry;
            const entt::registry& MeshRegistry;

            const GBufferSettings& Settings;
            GBufferStats& Stats;

            bool IsDepthPrePass = false;
        };

        void CreatePso(benzin::PsoId id, benzin::IndexOrder indexOrder, bool isDepthPrePass);

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

        void RenderMeshes(const MeshRenderContext& context, bool isIndexOrderClockwise) const;
        void RenderLights(const MeshRenderContext& context) const;
        void RenderMesh(const MeshRenderContext& context, const benzin::MeshComponent& meshComponent, const DirectX::XMMATRIX& localToWorldMatrix) const;

    private:
        const benzin::Scene& m_Scene;

        bool m_IsDepthPrePassEnabled = true;
    };

}
