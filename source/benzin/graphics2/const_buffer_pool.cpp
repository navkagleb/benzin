#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics2/const_buffer_pool.hpp"

#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/cmd_line_args.hpp"
#include "benzin/core/math.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    static std::pair<uint32_t, uint32_t> ParseSize(uint32_t sizeInBytes)
    {
        const uint32_t alignedSizeInBytes = AlignUp(sizeInBytes, GraphicsConfig::GetConstBufferAlignmentInBytes());
        const uint32_t poolIndex = alignedSizeInBytes / GraphicsConfig::GetConstBufferAlignmentInBytes();

        return { alignedSizeInBytes, poolIndex - 1 };
    }

    // 

    ConstBufferPool::ConstBufferPool(Device& device)
        : m_Device{ device }
    {}

    ConstBufferPool::~ConstBufferPool() = default;

    void ConstBufferPool::BeginFrame()
    {
        for (auto& [_, pool] : m_Pools)
        {
            pool.AllocatedCount = 0;
        }
    }

    void ConstBufferPool::PreAllocate(uint32_t sizeInBytes, uint32_t count)
    {
        const auto [_, poolIndex] = ParseSize(sizeInBytes);
        m_Pools[poolIndex].PreAllocatedElementCount += count;
    }

    uint64_t ConstBufferPool::Allocate(std::span<const std::byte> data)
    {
        const auto [alignedSizeInBytes, poolIndex] = ParseSize((uint32_t)data.size_bytes());

        auto& pool = m_Pools[poolIndex];
        BenzinAssert(pool.AllocatedCount < pool.PreAllocatedElementCount);

        const uint32_t poolElementCount = pool.PreAllocatedElementCount * CmdLineArgs::GetFrameInFlightCount();

        if (pool.BufferPool.get() == nullptr || pool.BufferPool->GetElementCount() != poolElementCount)
        {
            MakeUniquePtr(pool.BufferPool, m_Device, BufferCreation
            {
                .DebugName = std::format("ConstBuffer_{}", alignedSizeInBytes),
                .MemoryType = ResourceMemoryType::Upload,
                .Type = BufferType::Const,
                .ElementSizeInBytes = alignedSizeInBytes,
                .ElementCount = poolElementCount,
            });
        }

        const uint32_t elementIndexInBufferPool = m_Device.GetActiveFrameIndex() * pool.PreAllocatedElementCount + pool.AllocatedCount++;

        BufferWriter writer{ pool.BufferPool->GetCpuMappedData(), pool.BufferPool->GetSizeInBytes() };
        writer.SetElementPosition(elementIndexInBufferPool, alignedSizeInBytes);
        writer.WriteData(data);

        return pool.BufferPool->GetGpuVirtualAddress(elementIndexInBufferPool);
    }

}
