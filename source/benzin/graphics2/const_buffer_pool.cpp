#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics2/const_buffer_pool.hpp"

#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/cmd_line_args.hpp"
#include "benzin/core/math.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_heap.hpp"

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
        BenzinProfile();

        for (auto& [_, pool] : m_Pools)
        {
            pool.PreAllocatedElementCount = 0;
            pool.AllocatedElementCount = 0;
        }
    }

    void ConstBufferPool::EndFrame() const
    {
        BenzinProfile();

#if BENZIN_IS_ASSERTS_ENABLED
        for (const auto& [_, pool] : m_Pools)
        {
            BenzinAssert(pool.AllocatedElementCount == pool.PreAllocatedElementCount, "Some const buffers aren't used in the frame!");
        }
#endif
    }

    void ConstBufferPool::AllocatePools()
    {
        BenzinProfile();

        for (auto& [poolIndex, pool] : m_Pools)
        {
            if (pool.BufferPool.get() == nullptr || pool.MaxElementCount < pool.PreAllocatedElementCount)
            {
                pool.MaxElementCount = pool.PreAllocatedElementCount;

                const uint32_t alignedSizeInBytes = (poolIndex + 1) * GraphicsConfig::GetConstBufferAlignmentInBytes();
                MakeUniquePtr(pool.BufferPool, m_Device, BufferCreation
                {
                    .DebugName = std::format("ConstBuffer_{}", alignedSizeInBytes),
                    .HeapType = GpuHeapType::Upload,
                    .Type = BufferType::Const,
                    .ElementSizeInBytes = alignedSizeInBytes,
                    .ElementCount = pool.MaxElementCount * CmdLineArgs::GetFrameInFlightCount(),
                });
            }
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

        Pool& pool = m_Pools[poolIndex];
        BenzinAssert(pool.AllocatedElementCount < pool.PreAllocatedElementCount, "Make sure you have allocated all the necessary const buffers in advance!");

        const uint32_t elementIndexInBufferPool = m_Device.GetActiveFrameIndex() * pool.MaxElementCount + pool.AllocatedElementCount++;

        BufferWriter writer{ pool.BufferPool->GetCpuMappedData(), pool.BufferPool->GetSizeInBytes() };
        writer.SetElementPosition(elementIndexInBufferPool, alignedSizeInBytes);
        writer.WriteData(data);

        return pool.BufferPool->GetGpuVirtualAddress(elementIndexInBufferPool);
    }

}
