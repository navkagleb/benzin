#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/mapped_data.hpp"

#include "spieler/core/assert.hpp"

namespace spieler::renderer
{

    static constexpr uint32_t g_SubresourceIndex{ 0 };

    MappedData::MappedData(Resource& resource)
        : m_Resource{ resource }
    {
        SPIELER_ASSERT(SUCCEEDED(m_Resource.GetResource()->Map(g_SubresourceIndex, nullptr, reinterpret_cast<void**>(& m_Data))));
    }

    MappedData::~MappedData()
    {
        m_Resource.GetResource()->Unmap(g_SubresourceIndex, nullptr);
    }

} // namespace spieler::renderer