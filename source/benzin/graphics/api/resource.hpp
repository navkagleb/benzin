#pragma once

#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/descriptor_manager.hpp"

namespace benzin
{

    struct SubResourceData
    {
        const std::byte* Data{ nullptr };
        size_t RowPitch{ 0 };
        size_t SlicePitch{ 0 };
    };

    class Resource
    {
    public:
        BENZIN_NON_COPYABLE(Resource)
        BENZIN_NON_MOVEABLE(Resource)
        BENZIN_DX_DEBUG_NAME_IMPL(m_D3D12Resource)

    public:
        enum class State : std::underlying_type_t<D3D12_RESOURCE_STATES>
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
            GenericRead = D3D12_RESOURCE_STATE_GENERIC_READ,
        };

    protected:
        explicit Resource(Device& device);
        virtual ~Resource();

    public:
        ID3D12Resource* GetD3D12Resource() const { BENZIN_ASSERT(m_D3D12Resource); return m_D3D12Resource; }

        State GetCurrentState() const { return m_CurrentState; }
        void SetCurrentState(State resourceState) { m_CurrentState = resourceState; }

    public:
        uint64_t GetGPUVirtualAddress() const;
        uint32_t GetSizeInBytes() const;

        bool HasShaderResourceView(uint32_t index = 0) const;
        bool HasUnorderedAccessView(uint32_t index = 0) const;

        const Descriptor& GetShaderResourceView(uint32_t index = 0) const;
        const Descriptor& GetUnorderedAccessView(uint32_t index = 0) const;

    protected:
        bool HasResourceView(Descriptor::Type descriptorType, uint32_t index) const;
        const Descriptor& GetResourceView(Descriptor::Type descriptorType, uint32_t index) const;
        uint32_t PushResourceView(Descriptor::Type descriptorType, const Descriptor& descriptor);

    protected:
        Device& m_Device;

        ID3D12Resource* m_D3D12Resource{ nullptr };
        State m_CurrentState{ State::Present };

        std::unordered_map<Descriptor::Type, std::vector<Descriptor>> m_Views;
    };

} // namespace benzin
