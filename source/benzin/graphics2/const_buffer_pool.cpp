#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics2/const_buffer_pool.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/math.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    std::pair<uint32_t, uint32_t> ParseSize(uint32_t sizeInBytes)
    {
        const uint32_t constBufferAlignment = GfxConfig::s_ConstantBufferAlignment.GetByteCount();

        const uint32_t alignedSize = AlignUp(sizeInBytes, constBufferAlignment);
        const uint32_t poolIndex = alignedSize / constBufferAlignment;

        return { alignedSize, poolIndex - 1 };
    }

    // 

    ConstBufferPool::ConstBufferPool(Device& device)
        : m_Device{ device }
    {}

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
        const auto [alignedSize, poolIndex] = ParseSize((uint32_t)data.size_bytes());

        auto& pool = m_Pools[poolIndex];
        BenzinAssert(pool.AllocatedCount < pool.PreAllocatedElementCount);

        const uint32_t poolElementCount = pool.PreAllocatedElementCount * CommandLineArgs::GetU32("FrameInFlightCount");

        if (pool.BufferPool.get() == nullptr || pool.BufferPool->GetElementCount() != poolElementCount)
        {
            MakeUniquePtr(pool.BufferPool, m_Device, BufferCreation
            {
                .DebugName = std::format("ConstBuffer_{}", alignedSize),
                .MemoryType = ResourceMemoryType::Upload,
                .Type = BufferType::Constant,
                .ElementSize = alignedSize,
                .ElementCount = poolElementCount,
            });
        }

        const uint32_t elementIndexInBufferPool = m_Device.GetActiveFrameIndex() * pool.PreAllocatedElementCount + pool.AllocatedCount++;

        BufferWriter writer{ pool.BufferPool->GetCpuMappedData(), pool.BufferPool->GetSize() };
        writer.SetElementPosition(elementIndexInBufferPool, alignedSize);
        writer.WriteData(data);

        return pool.BufferPool->GetGpuVirtualAddress(elementIndexInBufferPool);
    }

}
