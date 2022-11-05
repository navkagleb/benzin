#pragma once

#include "spieler/renderer/resource.hpp"

namespace spieler::renderer
{

    class BufferResource : public Resource
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
        static std::shared_ptr<BufferResource> Create(Device& device, const Config& config);

    public:
        BufferResource() = default;
        BufferResource(Device& device, const Config& config);

    public:
        ~BufferResource() override = default;

    public:
        const Config& GetConfig() const { return m_Config; }

    public:
        GPUVirtualAddress GetGPUVirtualAddressWithOffset(uint64_t offset) const;

    protected:
        Config m_Config;
    };

    class Buffer
    {
    public:
        std::shared_ptr<BufferResource>& GetBufferResource() { return m_BufferResource; }
        const std::shared_ptr<BufferResource>& GetBufferResource() const { return m_BufferResource; }
        void SetBufferResource(std::shared_ptr<BufferResource>&& bufferResource) { m_BufferResource = std::move(bufferResource); }

    private:
        std::shared_ptr<BufferResource> m_BufferResource;
    };

} // namespace spieler::renderer
