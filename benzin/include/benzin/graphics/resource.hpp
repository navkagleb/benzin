#pragma once

#include "benzin/graphics/common.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    struct SubResourceData
    {
        const std::byte* Data{ nullptr };
        uint64_t RowPitch{ 0 };
        uint64_t SlicePitch{ 0 };
    };

    class Resource
    {
    public:
        enum class State : uint16_t
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

    public:
        BENZIN_NAME_D3D12_OBJECT(m_D3D12Resource)

    public:
        friend class Device;

    public:
        Resource() = default;

    protected:
        Resource(ID3D12Resource* d3d12Resource);

    public:
        virtual ~Resource();

    public:
        ID3D12Resource* GetD3D12Resource() const { return m_D3D12Resource; }

        State GetCurrentState() const { return m_CurrentState; }
        void SetCurrentState(State resourceState) { m_CurrentState = resourceState; }

    public:
        uint64_t GetGPUVirtualAddress() const;

        bool HasShaderResourceView(uint32_t index = 0) const;
        bool HasUnorderedAccessView(uint32_t index = 0) const;

        uint32_t PushShaderResourceView(const Descriptor& srv);
        uint32_t PushUnorderedAccessView(const Descriptor& uav);

        Descriptor GetShaderResourceView(uint32_t index = 0) const;
        Descriptor GetUnorderedAccessView(uint32_t index = 0) const;

        void ReleaseViews(DescriptorManager& descriptorManager);

    protected:
        bool HasResourceView(Descriptor::Type descriptorType, uint32_t index) const;
        uint32_t PushResourceView(Descriptor::Type descriptorType, const Descriptor& descriptor);
        Descriptor GetResourceView(Descriptor::Type descriptorType, uint32_t index) const;

    protected:
        ID3D12Resource* m_D3D12Resource{ nullptr };
        State m_CurrentState{ State::Present };

        std::unordered_map<Descriptor::Type, std::vector<Descriptor>> m_Views;
    };

} // namespace benzin
