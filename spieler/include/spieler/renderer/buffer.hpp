#pragma once

#include "spieler/renderer/resource.hpp"

namespace spieler::renderer
{

    enum class GPUVirtualAddress : uint64_t {};

    enum class BufferFlags : uint8_t
    {
        None = 0,
        Dynamic = 1 << 0,
        ConstantBuffer = 1 << 1
    };

    struct BufferConfig
    {
        uint64_t Alignment{ 0 };
        uint32_t ElementSize{ 0 };
        uint32_t ElementCount{ 0 };
    };

    class BufferResource : public Resource
    {
    public:
        BufferResource(const BufferConfig& config);

    public:
        GPUVirtualAddress GetGPUVirtualAddress() const;
        uint32_t GetStride() const;
        uint32_t GetSize() const;

    private:
        BufferConfig m_Config;
    };

} // namespace spieler::renderer