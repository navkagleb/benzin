#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{

    class Scene;

}

namespace sandbox
{

    struct GBufferStats;

    class GeometryPass : public benzin::RenderPass
    {
    public:
        explicit GeometryPass(const benzin::Scene& scene);
        ~GeometryPass() override;

    private:
        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

        void RenderMesh(entt::entity meshHandle, const DirectX::XMMATRIX& localToWorldMatrix) const;

    private:
        const benzin::Scene& m_Scene;
        GBufferStats& m_Stats;

        mutable uint32_t m_TransformIndex = 0;
        bool m_IsFrustumCullingEnabled = true;
    };

}
