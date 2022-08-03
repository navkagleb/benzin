#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/buffer.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "spieler/renderer/device.hpp"

#include "renderer/mapped_data.hpp"

namespace spieler::renderer
{

    BufferResource::BufferResource(Device& device, const Config& config)
        : m_Config{ config }
    {
        using namespace magic_enum::bitwise_operators;

        if ((m_Config.Flags & Flags::ConstantBuffer) != Flags::None)
        {
            m_Config.ElementSize = utils::Align<uint32_t>(m_Config.ElementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        }

        m_DX12Resource = device.CreateDX12Buffer(m_Config);
    }

    void BufferResource::Write(uint64_t offset, const void* data, uint64_t size)
    {
        MappedData mappedData{ *this, 0 };

        memcpy_s(mappedData.GetData() + offset, size, data, size);
    }

} // namespace spieler::renderer
