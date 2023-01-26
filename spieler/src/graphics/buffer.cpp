#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/buffer.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/utils.hpp"

namespace spieler
{

    static uint32_t g_BufferResourceCounter = 0;

    BufferResource::BufferResource(ID3D12Resource* d3d12Resource, const Config& config)
        : Resource{ d3d12Resource }
        , m_Config{ config }
    {
        SetName("BufferResource" + std::to_string(g_BufferResourceCounter));

        SPIELER_INFO("{} created", GetName());

        g_BufferResourceCounter++;
    }

    uint64_t BufferResource::GetGPUVirtualAddressWithOffset(uint64_t offset) const
    {
        SPIELER_ASSERT(m_D3D12Resource);

        return m_D3D12Resource->GetGPUVirtualAddress() + offset * m_Config.ElementSize;
    }

} // namespace spieler
