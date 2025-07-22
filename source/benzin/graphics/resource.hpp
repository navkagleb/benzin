#pragma once

#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    struct SubResourceData
    {
        const std::byte* Data = nullptr;

        uint64_t RowPitchInBytes = 0;
        uint64_t SlicePitchInBytes = 0;
    };

    enum class ResourceState : int32_t
    {
        Common = D3D12_RESOURCE_STATE_COMMON,
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
        GenericRead = D3D12_RESOURCE_STATE_GENERIC_READ,
        RayTracing_AccelerationStructure = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
    };

    class Resource
    {
    protected:
        explicit Resource(Device& device);
        virtual ~Resource();

        BenzinDefineNonCopyable(Resource);
        BenzinDefineNonMoveable(Resource);

    public:
        auto* GetD3D12Resource() const { return m_D3D12Resource; }

        auto GetCurrentState() const { return m_CurrentState; }
        void SetCurrentState(ResourceState resourceState) const { m_CurrentState = resourceState; }

        uint64_t GetAllocationSizeInBytes() const;

        virtual uint64_t GetSizeInBytes() const = 0;

    protected:
        template <typename T>
        Descriptor& GetViewDescriptor(const T& viewDesc) const
        {
            const size_t hash = std::hash<T>{}(viewDesc);
            return m_ViewDescriptors[hash];
        }

        const Descriptor& TryGetViewDescriptor(size_t hash, std::function<Descriptor()>&& createDescriptorCallback) const;

    protected:
        Device& m_Device;

        ID3D12Resource* m_D3D12Resource = nullptr;
        mutable ResourceState m_CurrentState = ResourceState::Common;

    private:
        mutable std::unordered_map<size_t, Descriptor> m_ViewDescriptors;
    };

}
