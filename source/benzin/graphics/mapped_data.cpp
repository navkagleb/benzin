#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/mapped_data.hpp"

namespace benzin
{

    namespace
    {

        constexpr uint32_t g_DefaultSubresourceIndex = 0;

    } // anonymous namespace

    MappedData::MappedData(const Buffer& buffer)
        : m_Buffer{ buffer }
    {
        ID3D12Resource* d3d12Resource = m_Buffer.GetD3D12Resource();
        BenzinAssert(d3d12Resource);

        BenzinAssert(d3d12Resource->Map(g_DefaultSubresourceIndex, nullptr, reinterpret_cast<void**>(&m_Data)));
    }

    MappedData::~MappedData()
    {   
        if (ID3D12Resource* d3d12Resource = m_Buffer.GetD3D12Resource())
        {
            d3d12Resource->Unmap(g_DefaultSubresourceIndex, nullptr);
        }
    }

} // namespace benzin
