#pragma once

namespace benzin
{
    class GraphicsCommandList;
    class RenderResources;
    class ResourceBarriers;
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

        void SetRenderTargets(benzin::GraphicsCommandList& cmdList) const;

        [[nodiscard]]
        benzin::ResourceBarriers CreateResourceBarriers(
            benzin::GraphicsCommandList& cmdList,
            benzin::ResourceState depthStencilState
        ) const;
    };

}
