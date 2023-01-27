#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/mapped_data.hpp"

namespace benzin
{

    MappedData::MappedData(Resource& resource, uint32_t subresourceIndex)
        : m_Resource{ resource }
        , m_SubresourceIndex{ subresourceIndex }
    {
        BENZIN_ASSERT(m_Resource.GetD3D12Resource());

        BENZIN_D3D12_ASSERT(m_Resource.GetD3D12Resource()->Map(m_SubresourceIndex, nullptr, reinterpret_cast<void**>(&m_Data)));
    }

    MappedData::~MappedData()
    {   
        if (m_Resource.GetD3D12Resource())
        {
            m_Resource.GetD3D12Resource()->Unmap(m_SubresourceIndex, nullptr);
        }
    }

    void MappedData::Write(const void* data, uint64_t size, uint64_t offset)
    {
        memcpy_s(m_Data + offset, size, data, size);
    }

} // namespace benzin
