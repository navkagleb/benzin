#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/buffer.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/utils.hpp"

namespace benzin
{

    static uint32_t g_BufferResourceCounter = 0;

    BufferResource::BufferResource(ID3D12Resource* d3d12Resource, const Config& config, const std::string& debugName)
        : Resource{ d3d12Resource }
        , m_Config{ config }
    {
        SetDebugName(debugName.empty() ? std::to_string(g_BufferResourceCounter) : debugName, true);
        g_BufferResourceCounter++;
    }

    uint64_t BufferResource::GetGPUVirtualAddressBeginWith(uint64_t beginElement) const
    {
        BENZIN_ASSERT(m_D3D12Resource);
        BENZIN_ASSERT(beginElement < m_Config.ElementCount);

        return m_D3D12Resource->GetGPUVirtualAddress() + beginElement * m_Config.ElementSize;
    }

} // namespace benzin
