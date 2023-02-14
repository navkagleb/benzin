#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/buffer.hpp"

namespace benzin
{

    BufferResource::BufferResource(ID3D12Resource* d3d12Resource, const Config& config, std::string_view debugName)
        : Resource{ d3d12Resource }
        , m_Config{ config }
    {
        SetDebugName(debugName, true);
    }

    uint64_t BufferResource::GetGPUVirtualAddressBeginWith(uint64_t beginElement) const
    {
        BENZIN_ASSERT(m_D3D12Resource);
        BENZIN_ASSERT(beginElement < m_Config.ElementCount);

        return m_D3D12Resource->GetGPUVirtualAddress() + beginElement * m_Config.ElementSize;
    }

} // namespace benzin
