#pragma once

namespace benzin
{
    class GraphicsCmdList;
    class RenderResources;
    class ScopedResourceBarriers;
    class Texture;

    enum class ResourceState : int; // TODO
}

namespace sandbox
{

    struct GBuffer
    {
        const benzin::Texture& AlbedoAndRoughness;
        const benzin::Texture& EmissiveAndMetallic;
        const benzin::Texture& WorldNormal;
        const benzin::Texture& Mv;
        const benzin::Texture& ViewDepth;
        const benzin::Texture& DepthStencil;

        explicit GBuffer(const benzin::RenderResources& resources);

        void SetRenderTargets(benzin::GraphicsCmdList& cmdList) const;
        void SetDepthStencilOnly(benzin::GraphicsCmdList& cmdList) const;

        void ClearRenderTargets(benzin::GraphicsCmdList& cmdList) const;
        void ClearDepthStencil(benzin::GraphicsCmdList& cmdList) const;

        [[nodiscard]]
        benzin::ScopedResourceBarriers CreateResourceBarriers(
            benzin::GraphicsCmdList& cmdList,
            benzin::ResourceState depthStencilState,
            bool isDepthStencilOnly = false
        ) const;
    };

}
