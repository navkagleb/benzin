#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/mapped_data.hpp"

#include "benzin/core/asserter.hpp"

namespace benzin
{

    static constexpr uint32_t g_DefaultSubresourceIndex = 0;

    //

    MappedData::MappedData(const Buffer& buffer)
        : m_Buffer{ buffer }
    {
        ID3D12Resource* d3d12Resource = m_Buffer.GetD3D12Resource();
        BenzinAssert(d3d12Resource);

        const D3D12_RANGE d3d12Range{ .Begin = 0, .End = 0 }; // The use of 'm_Data' is for writing only
        BenzinAssert(d3d12Resource->Map(g_DefaultSubresourceIndex, &d3d12Range, reinterpret_cast<void**>(&m_Data)));
    }

    MappedData::~MappedData()
    {   
        if (ID3D12Resource* d3d12Resource = m_Buffer.GetD3D12Resource())
        {
            d3d12Resource->Unmap(g_DefaultSubresourceIndex, nullptr);
        }
    }

} // namespace benzin
