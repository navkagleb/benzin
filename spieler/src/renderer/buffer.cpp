#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/buffer.hpp"

namespace spieler::renderer
{

    BufferResource::BufferResource(const BufferConfig& config)
        : m_Config(config)
    {}

    GPUVirtualAddress BufferResource::GetGPUVirtualAddress() const
    {
        return static_cast<GPUVirtualAddress>(m_Resource->GetGPUVirtualAddress());
    }

    uint32_t BufferResource::GetStride() const
    {
        return m_Config.ElementSize;
    }

    uint32_t BufferResource::GetSize() const
    {
        return m_Config.ElementSize * m_Config.ElementCount;
    }

} // namespace spieler::renderer