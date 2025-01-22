#pragma once

namespace benzin
{

    class Buffer;
    class Device;

    class ConstBufferPool
    {
    public:
        explicit ConstBufferPool(Device& device);

        void BeginFrame();

        void PreAllocate(uint32_t sizeInBytes);
        uint64_t Allocate(std::span<const std::byte> data);

        template <typename T>
        void PreAllocate()
        {
            PreAllocate(sizeof(T));
        }

        template <typename T>
        uint64_t Allocate(const T& data)
        {
            return Allocate(std::as_bytes(std::span{ &data, 1 }));
        }

    private:
        Device& m_Device;

        struct Pool
        {
            uint32_t PreAllocatedElementCount = 0;
            uint32_t AllocatedCount = 0;

            std::unique_ptr<Buffer> BufferPool;
        };

        std::unordered_map<uint32_t, Pool> m_Pools;
    };

}
