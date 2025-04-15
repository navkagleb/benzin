#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/resource.hpp"

#include "benzin/graphics/device.hpp"

namespace benzin
{

    Resource::Resource(Device& device)
        : m_Device{ device }
    {}

    Resource::~Resource()
    {
        for (const auto& [_, descriptor] : m_ViewDescriptors)
        {
            m_Device.DeferredRelease(descriptor);
        }
        m_ViewDescriptors.clear();

        m_Device.DeferredRelease(m_D3D12Resource);
    }

    Bytes32 Resource::GetAllocationSize() const
    {
        BenzinAssert(m_D3D12Resource != nullptr);

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = m_D3D12Resource->GetDesc();
        const D3D12_RESOURCE_ALLOCATION_INFO d3d12ResourceAllocationInfo = m_Device.GetD3D12Device()->GetResourceAllocationInfo(0, 1, &d3d12ResourceDesc);

        BenzinAssert(d3d12ResourceAllocationInfo.SizeInBytes <= std::numeric_limits<uint32_t>::max());
        return (uint32_t)d3d12ResourceAllocationInfo.SizeInBytes;
    }

    const Descriptor& Resource::TryGetViewDescriptor(size_t hash, std::function<Descriptor()>&& createDescriptorCallback) const
    {
        const auto [it, _] = m_ViewDescriptors.try_emplace(
            hash,
            MakeLazyConverter(std::move(createDescriptorCallback))
        );

        return (*it).second;
    }

}
