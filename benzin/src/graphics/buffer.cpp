#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/buffer.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/utils.hpp"

namespace benzin
{

    static uint32_t g_BufferResourceCounter = 0;

    BufferResource::BufferResource(ID3D12Resource* d3d12Resource, const Config& config)
        : Resource{ d3d12Resource }
        , m_Config{ config }
    {
        SetName("BufferResource" + std::to_string(g_BufferResourceCounter));

        BENZIN_INFO("{} created", GetName());

        g_BufferResourceCounter++;
    }

    uint64_t BufferResource::GetGPUVirtualAddressWithOffset(uint64_t offset) const
    {
        BENZIN_ASSERT(m_D3D12Resource);

        return m_D3D12Resource->GetGPUVirtualAddress() + offset * m_Config.ElementSize;
    }

} // namespace benzin
