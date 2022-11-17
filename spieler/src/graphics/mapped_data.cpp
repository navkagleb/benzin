#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/mapped_data.hpp"

namespace spieler
{

    MappedData::MappedData(Resource* resource, uint32_t subresourceIndex)
        : m_Resource{ resource }
        , m_SubresourceIndex{ subresourceIndex }
    {
        SPIELER_ASSERT(m_Resource->GetDX12Resource());

        const ::HRESULT result{ m_Resource->GetDX12Resource()->Map(m_SubresourceIndex, nullptr, reinterpret_cast<void**>(&m_Data)) };

        SPIELER_ASSERT(SUCCEEDED(result));
    }

    MappedData::MappedData(const std::shared_ptr<Resource>& resource, uint32_t subresourceIndex)
        : m_Resource{ resource.get() }
        , m_SubresourceIndex{ subresourceIndex }
    {
        SPIELER_ASSERT(m_Resource->GetDX12Resource());

        const ::HRESULT result{ m_Resource->GetDX12Resource()->Map(m_SubresourceIndex, nullptr, reinterpret_cast<void**>(&m_Data)) };

        SPIELER_ASSERT(SUCCEEDED(result));
    }

    MappedData::~MappedData()
    {   
        if (m_Resource->GetDX12Resource())
        {
            m_Resource->GetDX12Resource()->Unmap(m_SubresourceIndex, nullptr);
        }
    }

    void MappedData::Write(const void* data, uint64_t size, uint64_t offset)
    {
        memcpy_s(m_Data + offset, size, data, size);
    }

} // namespace spieler
