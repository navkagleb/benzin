#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/buffer.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/utils.hpp"

namespace spieler
{

    namespace _internal
    {

        BufferResource::Config ValidateConfig(const BufferResource::Config& config)
        {
            using namespace magic_enum::bitwise_operators;

            BufferResource::Config validatedConfig{ config };

            if ((validatedConfig.Flags & BufferResource::Flags::ConstantBuffer) != BufferResource::Flags::None)
            {
                validatedConfig.ElementSize = utils::Align<uint32_t>(validatedConfig.ElementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            }

            return validatedConfig;
        }

    } // namespace _internal

    std::shared_ptr<BufferResource> BufferResource::Create(Device& device, const Config& config)
    {
        return std::make_shared<BufferResource>(device, config);
    }

    BufferResource::BufferResource(Device& device, const Config& config)
        : Resource{ device.CreateDX12Buffer(_internal::ValidateConfig(config)) }
        , m_Config{ _internal::ValidateConfig(config) }
    {}

    GPUVirtualAddress BufferResource::GetGPUVirtualAddressWithOffset(uint64_t offset) const
    {
        SPIELER_ASSERT(m_DX12Resource);

        return m_DX12Resource->GetGPUVirtualAddress() + offset * m_Config.ElementSize;
    }

} // namespace spieler
