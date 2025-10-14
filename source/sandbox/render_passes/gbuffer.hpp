#pragma once

namespace benzin
{
    class GraphicsCmdList;
    class RenderResources;
    class ScopedResourceBarriers;
    class Texture;

    enum class ResourceState : int32_t;
}

namespace sandbox
{

    struct GBuffer
    {
        const benzin::Texture& m_AlbedoAndRoughness;
        const benzin::Texture& m_EmissiveAndMetallic;
        const benzin::Texture& m_WorldNormal;
        const benzin::Texture& m_Mv;
        const benzin::Texture& m_ViewDepth;
        const benzin::Texture& m_DepthStencil;

        explicit GBuffer(const benzin::RenderResources& resources);

        void SetRenderTargets(benzin::GraphicsCmdList& cmdList) const;
        void SetDepthStencilOnly(benzin::GraphicsCmdList& cmdList) const;

        void ClearRenderTargets(benzin::GraphicsCmdList& cmdList) const;
        void ClearDepthStencil(benzin::GraphicsCmdList& cmdList) const;

        [[nodiscard]]
        benzin::ScopedResourceBarriers CreateResourceBarriers(
            benzin::GraphicsCmdList& cmdList,
            benzin::ResourceState depthStencilState,
            bool isDepthStencilOnly = false) const;
    };

}
