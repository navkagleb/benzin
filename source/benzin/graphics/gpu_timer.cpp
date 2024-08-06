#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/gpu_timer.hpp"

// Ref: https://devblogs.microsoft.com/pix/winpixeventruntime/
#define USE_PIX
#include <pix3.h>
#pragma comment(lib, "WinPixEventRuntime.lib")

// #TODO: Take a PIX capture:
// Ref: https://devblogs.microsoft.com/pix/taking-a-capture/

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    // GpuTimer

    GpuTimer::GpuTimer(Device& device, uint32_t timerCount)
        : m_InverseFrequency{ 1.0f / device.GetGraphicsCommandQueue().GetTimestampFrequency() }
        , m_ReadbackLatency{ CommandLineArgs::GetU32("FrameInFlightCount") + 1}
        , m_ProfiledCommandList{ device.GetGraphicsCommandQueue().GetCommandList() }
        , m_ReadbackBuffer{ device }
    {
        BenzinAssert(timerCount <= 32);

        m_Timestamps.resize(timerCount * 2);

        const D3D12_QUERY_HEAP_DESC d3d12QueryHeapDesc
        {
            .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
            .Count = (uint32_t)m_Timestamps.size(),
            .NodeMask = 0,
        };

        BenzinEnsure(device.GetD3D12Device()->CreateQueryHeap(&d3d12QueryHeapDesc, IID_PPV_ARGS(&m_D3D12TimestampQueryHeap)));
        SetDxObjectDebugName(m_D3D12TimestampQueryHeap, "GpuTimer_TimestampQueryHeap");

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
        BenzinSafeDxObjectRelease(m_D3D12TimestampQueryHeap);
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

        {
            BenzinPushGpuEvent(m_ProfiledCommandList, "ResolveGpuTimestamps");

            // Otherwise 'ResolveQueryData' will send Error message
            ForceProfileUnprofiledTimers();

            m_ProfiledCommandList.GetD3D12GraphicsCommandList()->ResolveQueryData(
                m_D3D12TimestampQueryHeap,
                D3D12_QUERY_TYPE_TIMESTAMP,
                0,
                (uint32_t)m_Timestamps.size(),
                m_ReadbackBuffer.GetD3D12Resource(),
                resolveFrameIndex * m_ReadbackBuffer.GetElementSize()
            );
        }

        const size_t readbackBufferOffset = readbackFrameIndex * m_ReadbackBuffer.GetElementSize();
        const D3D12_RANGE d3d12ReadbackRange
        {
            .Begin = readbackBufferOffset,
            .End = readbackBufferOffset + m_ReadbackBuffer.GetElementSize(),
        };

        uint64_t* timestampData = nullptr;
        BenzinEnsure(m_ReadbackBuffer.GetD3D12Resource()->Map(0, &d3d12ReadbackRange, reinterpret_cast<void**>(&timestampData)));
        BenzinExecuteOnScopeExit([this] { m_ReadbackBuffer.GetD3D12Resource()->Unmap(0, nullptr); });

        memcpy(m_Timestamps.data(), timestampData, m_ReadbackBuffer.GetElementSize());
    }

    void GpuTimer::EndQuery(uint32_t timestampIndex)
    {
        m_ProfiledCommandList.GetD3D12GraphicsCommandList()->EndQuery(m_D3D12TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, timestampIndex);
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

    // GpuEventTracker

    void GpuEventTracker::BeginEvent(const GraphicsCommandList& commandList, std::string_view eventName)
    {
        PIXBeginEvent(commandList.GetD3D12GraphicsCommandList(), PIX_COLOR_DEFAULT, "%s", eventName.data());
    }

    void GpuEventTracker::EndEvent(const GraphicsCommandList& commandList)
    {
        PIXEndEvent(commandList.GetD3D12GraphicsCommandList());
    }

} // namespace benzin
