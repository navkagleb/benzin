#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/resource.hpp"

#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    Resource::Resource(ID3D12Resource* d3d12Resource)
        : m_D3D12Resource{ d3d12Resource }
    {
        D3D12_HEAP_PROPERTIES d3d12HeapProperties{};
        BENZIN_D3D12_ASSERT(m_D3D12Resource->GetHeapProperties(&d3d12HeapProperties, nullptr));

        m_CurrentState = d3d12HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD ? State::GenericRead : State::Present;
    }

    Resource::~Resource()
    {
        const std::string name = GetName();

        SafeReleaseD3D12Object(m_D3D12Resource);

        BENZIN_INFO("{} destroyed", name);
    }

    uint64_t Resource::GetGPUVirtualAddress() const
    {
        BENZIN_ASSERT(m_D3D12Resource);

        return m_D3D12Resource->GetGPUVirtualAddress();
    }

    bool Resource::HasShaderResourceView(uint32_t index) const
    {
        return HasResourceView(Descriptor::Type::ShaderResourceView, index);
    }

    bool Resource::HasUnorderedAccessView(uint32_t index) const
    {
        return HasResourceView(Descriptor::Type::UnorderedAccessView, index);
    }

    uint32_t Resource::PushShaderResourceView(const Descriptor& srv)
    {
        return PushResourceView(Descriptor::Type::ShaderResourceView, srv);
    }

    uint32_t Resource::PushUnorderedAccessView(const Descriptor& uav)
    {
        return PushResourceView(Descriptor::Type::UnorderedAccessView, uav);
    }

    Descriptor Resource::GetShaderResourceView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::ShaderResourceView, index);
    }

    Descriptor Resource::GetUnorderedAccessView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::UnorderedAccessView, index);
    }

    void Resource::ReleaseViews(DescriptorManager& descriptorManager)
    {
        BENZIN_INFO("Release views of {}", GetName());

        for (const auto& [descriptorType, descriptors] : m_Views)
        {
            for (const auto& descriptor : descriptors)
            {
                descriptorManager.DeallocateDescriptor(descriptorType, descriptor);
            }
        }
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

    Descriptor Resource::GetResourceView(Descriptor::Type descriptorType, uint32_t index) const
    {
        BENZIN_ASSERT(m_Views.contains(descriptorType));

        const auto& descriptors = m_Views.at(descriptorType);
        BENZIN_ASSERT(index < descriptors.size());

        return descriptors[index];
    }


} // namespace benzin
