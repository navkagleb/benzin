#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/mapped_data.hpp"

namespace benzin
{

    namespace
    {

        constexpr uint32_t g_DefaultSubresourceIndex = 0;

    } // anonymous namespace

    MappedData::MappedData(BufferResource& bufferResource)
        : m_BufferResource{ bufferResource }
    {
        ID3D12Resource* d3d12Resource = m_BufferResource.GetD3D12Resource();
        BENZIN_ASSERT(d3d12Resource);

        BENZIN_D3D12_ASSERT(d3d12Resource->Map(g_DefaultSubresourceIndex, nullptr, reinterpret_cast<void**>(&m_Data)));
    }

    MappedData::~MappedData()
    {   
        if (ID3D12Resource* d3d12Resource = m_BufferResource.GetD3D12Resource())
        {
            d3d12Resource->Unmap(g_DefaultSubresourceIndex, nullptr);
        }
    }

    void MappedData::Write(const void* data, uint64_t size, uint64_t offset)
    {
#if BENZIN_DEBUG
        const uint64_t bufferSize = m_BufferResource.GetConfig().ElementSize * m_BufferResource.GetConfig().ElementCount;
        BENZIN_ASSERT(offset < bufferSize);
#endif

        memcpy_s(m_Data + offset, size, data, size);
    }

} // namespace benzin
