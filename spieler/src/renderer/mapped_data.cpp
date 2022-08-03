#include "spieler/config/bootstrap.hpp"

#include "renderer/mapped_data.hpp"

#include "spieler/core/assert.hpp"

namespace spieler::renderer
{

    MappedData::MappedData(Resource& resource, uint32_t subresourceIndex)
        : m_Resource{ resource }
        , m_SubresourceIndex{ subresourceIndex }
    {
        const ::HRESULT result{ m_Resource.GetDX12Resource()->Map(m_SubresourceIndex, nullptr, reinterpret_cast<void**>(&m_Data)) };

        SPIELER_ASSERT(SUCCEEDED(result));
    }

    MappedData::~MappedData()
    {   
        if (m_Resource.GetDX12Resource())
        {
            m_Resource.GetDX12Resource()->Unmap(m_SubresourceIndex, nullptr);
        }
    }

} // namespace spieler::renderer
