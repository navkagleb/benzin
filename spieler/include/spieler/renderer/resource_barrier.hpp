#pragma once

namespace spieler::renderer
{

    class Resource;

    enum class ResourceState : uint16_t
    {
        Present = D3D12_RESOURCE_STATE_COMMON,
        VertexBuffer = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        ConstantBuffer = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        IndexBuffer = D3D12_RESOURCE_STATE_INDEX_BUFFER,
        RenderTarget = D3D12_RESOURCE_STATE_RENDER_TARGET,
        UnorderedAccess = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        DepthWrite = D3D12_RESOURCE_STATE_DEPTH_WRITE,
        DepthRead = D3D12_RESOURCE_STATE_DEPTH_READ,
        NonPixelShaderResource = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        PixelShaderResource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        CopyDestination = D3D12_RESOURCE_STATE_COPY_DEST,
        CopySource = D3D12_RESOURCE_STATE_COPY_SOURCE,
        ResolveDestination = D3D12_RESOURCE_STATE_RESOLVE_DEST,
        ResolveSource = D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
        GenericRead = D3D12_RESOURCE_STATE_GENERIC_READ
    };

    struct TransitionResourceBarrier
    {
        const Resource* Resource{ nullptr };

        ResourceState From{ ResourceState::Present };
        ResourceState To{ ResourceState::Present };
    };

} // namespace spieler::renderer