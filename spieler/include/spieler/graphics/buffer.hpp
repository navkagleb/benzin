#pragma once

#include "spieler/graphics/resource.hpp"

namespace spieler
{

    class BufferResource final : public Resource
    {
    public:
        friend class Device;

    public:
        enum class Flags : uint8_t
        {
            None = 0,
            Dynamic = 1 << 0,
            ConstantBuffer = 1 << 1
        };

        struct Config
        {
            uint32_t Alignment{ 0 };
            uint32_t ElementSize{ 0 };
            uint32_t ElementCount{ 0 };
            Flags Flags{ Flags::None };
        };

    public:
        BufferResource() = default;

    private:
        BufferResource(ID3D12Resource* d3d12Resource, const Config& config);

    public:
        const Config& GetConfig() const { return m_Config; }

    public:
        uint64_t GetGPUVirtualAddressWithOffset(uint64_t offset) const;

    private:
        Config m_Config;
    };

} // namespace spieler
