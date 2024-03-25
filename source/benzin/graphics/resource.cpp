#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/resource.hpp"

#include "benzin/core/asserter.hpp"

namespace benzin
{

    Resource::Resource(Device& device)
        : m_Device{ device }
    {}

    Resource::~Resource()
    {
        for (const auto& [_, descriptor] : m_ViewDescriptors)
        {
            m_Device.GetDescriptorManager().FreeDescriptor(descriptor);
        }

        m_ViewDescriptors.clear();

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

} // namespace benzin
