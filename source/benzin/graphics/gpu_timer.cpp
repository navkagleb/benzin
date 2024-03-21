#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/gpu_timer.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_list.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    GpuTimer::GpuTimer(Device& device, const GpuTimerCreation& creation)
        : m_InverseFrequency{ 1.0f / creation.TimestampFrequency }
        , m_ReadbackLatency{ CommandLineArgs::GetFrameInFlightCount() + 1 }
        , m_ProfiledD3D12GraphicsCommandList{ creation.CommandList.GetD3D12GraphicsCommandList() }
        , m_ReadbackBuffer{ device }
    {
        BenzinAssert(creation.TimerCount < 32);

        m_Timestamps.resize(creation.TimerCount * 2);

        const D3D12_QUERY_HEAP_DESC d3d12QueryHeapDesc
        {
            .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
            .Count = (uint32_t)m_Timestamps.size(),
            .NodeMask = 0,
        };

        BenzinAssert(device.GetD3D12Device()->CreateQueryHeap(&d3d12QueryHeapDesc, IID_PPV_ARGS(&m_D3D12TimestampQueryHeap)));
        SetD3D12ObjectDebugName(m_D3D12TimestampQueryHeap, "GpuTimer_TimestampQueryHeap");

        m_ReadbackBuffer.Create(BufferCreation
        {
            .DebugName = "GpuTimer_ReadbackBuffer",
            .ElementSize = sizeof(uint64_t) * (uint32_t)m_Timestamps.size(),
            .ElementCount = m_ReadbackLatency,
            .Flags = BufferFlag::ReadbackBuffer,
            .InitialState = ResourceState::CopyDestination,
        });
    }

    GpuTimer::~GpuTimer()
    {
        SafeUnknownRelease(m_D3D12TimestampQueryHeap);
    }

    void GpuTimer::BeginProfile(uint32_t timerIndex)
    {
        BenzinAssert(timerIndex < m_Timestamps.size() / 2);
        EndQuery(timerIndex * 2);
    }

    void GpuTimer::EndProfile(uint32_t timerIndex)
    {
        BenzinAssert(timerIndex < m_Timestamps.size() / 2);
        EndQuery(timerIndex * 2 + 1);

        m_ProfiledTimers |= 1 << timerIndex;
    }

    std::chrono::microseconds GpuTimer::GetElapsedTime(uint32_t timerIndex) const
    {
        BenzinAssert(timerIndex < m_Timestamps.size() / 2);

        const auto startTimeStamp = m_Timestamps[timerIndex * 2];
        const auto endTimeStamp = m_Timestamps[timerIndex * 2 + 1];

        if (endTimeStamp < startTimeStamp)
        {
            return std::chrono::microseconds::zero();
        }

        return SecToUs((endTimeStamp - startTimeStamp) * m_InverseFrequency);
    }

    void GpuTimer::ResolveTimestamps(uint64_t cpuFrameIndex)
    {
        const uint32_t resolveFrameIndex = cpuFrameIndex % m_ReadbackLatency;
        const uint32_t readbackFrameIndex = (cpuFrameIndex + 1) % m_ReadbackLatency;

        // Otherwise 'ResolveQueryData' will send Error message
        ForceProfileUnprofiledTimers(); 

        m_ProfiledD3D12GraphicsCommandList->ResolveQueryData(
            m_D3D12TimestampQueryHeap,
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            (uint32_t)m_Timestamps.size(),
            m_ReadbackBuffer.GetD3D12Resource(),
            resolveFrameIndex * m_ReadbackBuffer.GetElementSize()
        );

        const size_t readbackBufferOffset = readbackFrameIndex * m_ReadbackBuffer.GetElementSize();
        const D3D12_RANGE d3d12ReadbackRange
        {
            .Begin = readbackBufferOffset,
            .End = readbackBufferOffset + m_ReadbackBuffer.GetElementSize(),
        };

        uint64_t* timestampData = nullptr;
        BenzinAssert(m_ReadbackBuffer.GetD3D12Resource()->Map(0, &d3d12ReadbackRange, reinterpret_cast<void**>(&timestampData)));
        BenzinExecuteOnScopeExit([this] { m_ReadbackBuffer.GetD3D12Resource()->Unmap(0, nullptr); });

        memcpy(m_Timestamps.data(), timestampData, m_ReadbackBuffer.GetElementSize());
    }

    void GpuTimer::EndQuery(uint32_t timestampIndex)
    {
        m_ProfiledD3D12GraphicsCommandList->EndQuery(m_D3D12TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, timestampIndex);
    }

    void GpuTimer::ForceProfileUnprofiledTimers()
    {
        for (const uint32_t timerIndex : std::views::iota(0u, m_Timestamps.size() / 2))
        {
            const bool isTimerProfiled = m_ProfiledTimers & (1 << timerIndex);
            if (!isTimerProfiled)
            {
                BeginProfile(timerIndex);
                EndProfile(timerIndex);
            }
        }

        m_ProfiledTimers = 0;
    }

} // namespace benzin
