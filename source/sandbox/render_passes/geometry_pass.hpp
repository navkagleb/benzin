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
        static void CreateGeometryPso(benzin::PsoId id, bool isMeshPipeline);

        ID3D12CommandSignature* m_D3D12DrawIndirectCmdSignature = nullptr;
        ID3D12CommandSignature* m_D3D12DispatchMeshIndirectCmdSignature = nullptr;
    };

    struct GBuffer
    {
        const benzin::Texture& m_AlbedoAndRoughness;
        const benzin::Texture& m_EmissiveAndMetallic;
        const benzin::Texture& m_WorldNormal;
        const benzin::Texture& m_Mv;
        const benzin::Texture& m_ViewDepth;
        const benzin::Texture& m_DepthStencil;

        explicit GBuffer(const benzin::RenderResources& resources);
    };

}
