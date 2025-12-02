#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/resource.hpp>

#include <benzin/graphics/device.hpp>

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
        m_D3D12Resource = nullptr;
    }

    const Descriptor& Resource::TryGetViewDescriptor(size_t hash, std::function<Descriptor()>&& createDescriptorCallback) const
    {
        const auto [it, _] = m_ViewDescriptors.try_emplace(
            hash,
            MakeLazyConverter(std::move(createDescriptorCallback)));

        return (*it).second;
    }

}
