#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/resource.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

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

        SafeUnknownRelease(m_D3D12Resource);
    }

    uint32_t Resource::GetAllocationSizeInBytes() const
    {
        BenzinAssert(m_D3D12Resource);

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = m_D3D12Resource->GetDesc();
        const D3D12_RESOURCE_ALLOCATION_INFO d3d12ResourceAllocationInfo = m_Device.GetD3D12Device()->GetResourceAllocationInfo(0, 1, &d3d12ResourceDesc);

        BenzinAssert(d3d12ResourceAllocationInfo.SizeInBytes <= std::numeric_limits<uint32_t>::max());
        return (uint32_t)d3d12ResourceAllocationInfo.SizeInBytes;
    }

    bool Resource::HasResourceView(DescriptorType descriptorType, uint32_t index) const
    {
        if (!m_Views.contains(descriptorType))
        {
            return false;
        }

        return index < m_Views.at(descriptorType).size();
    }

    uint32_t Resource::PushResourceView(DescriptorType descriptorType, const Descriptor& descriptor)
    {
        BenzinAssert(descriptor.IsValid());

        auto& descriptors = m_Views[descriptorType];
        descriptors.push_back(descriptor);

        return static_cast<uint32_t>(descriptors.size()) - 1;
    }

    const Descriptor& Resource::GetResourceView(DescriptorType descriptorType, uint32_t index) const
    {
        BenzinAssert(m_Views.contains(descriptorType));

        const auto& descriptors = m_Views.at(descriptorType);
        BenzinAssert(index < descriptors.size());

        return descriptors[index];
    }

} // namespace benzin
