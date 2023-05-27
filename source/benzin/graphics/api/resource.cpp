#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/resource.hpp"

#include "benzin/graphics/api/descriptor_manager.hpp"

namespace benzin
{

    Resource::Resource(Device& device)
        : m_Device{ device }
    {}

    Resource::~Resource()
    {
        for (const auto& [descriptorType, descriptors] : m_Views)
        {
            for (const auto& descriptor : descriptors)
            {
                m_Device.GetDescriptorManager().DeallocateDescriptor(descriptorType, descriptor);
            }
        }

        dx::SafeRelease(m_D3D12Resource);
    }

    uint64_t Resource::GetGPUVirtualAddress() const
    {
        BENZIN_ASSERT(m_D3D12Resource);

        return m_D3D12Resource->GetGPUVirtualAddress();
    }

    uint32_t Resource::GetSizeInBytes() const
    {
        BENZIN_ASSERT(m_D3D12Resource);

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = m_D3D12Resource->GetDesc();

        D3D12_RESOURCE_ALLOCATION_INFO1 d3d12ResourceAllocationInfo1;

        m_Device.GetD3D12Device()->GetResourceAllocationInfo1(0, 1, &d3d12ResourceDesc, &d3d12ResourceAllocationInfo1);

        return static_cast<uint32_t>(d3d12ResourceAllocationInfo1.SizeInBytes);
    }

    bool Resource::HasShaderResourceView(uint32_t index) const
    {
        return HasResourceView(Descriptor::Type::ShaderResourceView, index);
    }

    bool Resource::HasUnorderedAccessView(uint32_t index) const
    {
        return HasResourceView(Descriptor::Type::UnorderedAccessView, index);
    }

    const Descriptor& Resource::GetShaderResourceView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::ShaderResourceView, index);
    }

    const Descriptor& Resource::GetUnorderedAccessView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::UnorderedAccessView, index);
    }

    bool Resource::HasResourceView(Descriptor::Type descriptorType, uint32_t index) const
    {
        if (!m_Views.contains(descriptorType))
        {
            return false;
        }

        return index < m_Views.at(descriptorType).size();
    }

    uint32_t Resource::PushResourceView(Descriptor::Type descriptorType, const Descriptor& descriptor)
    {
        BENZIN_ASSERT(descriptor.IsValid());

        auto& descriptors = m_Views[descriptorType];
        descriptors.push_back(descriptor);

        return static_cast<uint32_t>(descriptors.size()) - 1;
    }

    const Descriptor& Resource::GetResourceView(Descriptor::Type descriptorType, uint32_t index) const
    {
        BENZIN_ASSERT(m_Views.contains(descriptorType));

        const auto& descriptors = m_Views.at(descriptorType);
        BENZIN_ASSERT(index < descriptors.size());

        return descriptors[index];
    }

} // namespace benzin
