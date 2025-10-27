#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/geometry_resources.hpp>

namespace benzin
{
    enum class PsoId : uint32_t;
}

namespace sandbox
{

    class GeometryPass : public benzin::RenderPass
    {
    public:
        GeometryPass();
        ~GeometryPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnRender() const override;

    private:
        void CreateGeometryPso(benzin::PsoId id, bool isMeshPipeline);

        ID3D12CommandSignature* m_D3D12DrawIndexedIndirectCmdSignature = nullptr;
    };

}
