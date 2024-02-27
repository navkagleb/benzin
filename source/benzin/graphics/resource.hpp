#pragma once

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    struct SubResourceData
    {
        const std::byte* Data = nullptr;

        size_t RowPitch = 0;
        size_t SlicePitch = 0;
    };

    class Resource
    {

    protected:
        explicit Resource(Device& device);
        virtual ~Resource();

        BenzinDefineNonCopyable(Resource);
        BenzinDefineNonMoveable(Resource);

    public:
        ID3D12Resource* GetD3D12Resource() const { return m_D3D12Resource; }

        ResourceState GetCurrentState() const { return m_CurrentState; }
        void SetCurrentState(ResourceState resourceState) { m_CurrentState = resourceState; }

    public:
        uint32_t GetAllocationSizeInBytes() const;

        virtual uint32_t GetSizeInBytes() const = 0;

        // These views have resources of any type
        bool HasShaderResourceView(uint32_t index = 0) const { return HasResourceView(DescriptorType::ShaderResourceView, index); }
        bool HasUnorderedAccessView(uint32_t index = 0) const { return HasResourceView(DescriptorType::UnorderedAccessView, index); }

        const Descriptor& GetShaderResourceView(uint32_t index = 0) const { return GetResourceView(DescriptorType::ShaderResourceView, index); }
        const Descriptor& GetUnorderedAccessView(uint32_t index = 0) const { return GetResourceView(DescriptorType::UnorderedAccessView, index); }

    protected:
        bool HasResourceView(DescriptorType descriptorType, uint32_t index) const;
        const Descriptor& GetResourceView(DescriptorType descriptorType, uint32_t index) const;
        uint32_t PushResourceView(DescriptorType descriptorType, const Descriptor& descriptor);

    protected:
        Device& m_Device;

        ID3D12Resource* m_D3D12Resource = nullptr;
        ResourceState m_CurrentState = ResourceState::Common;

        std::unordered_map<DescriptorType, std::vector<Descriptor>> m_Views;
    };

} // namespace benzin
