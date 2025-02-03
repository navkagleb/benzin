#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

#include <benzin/core/asserter.hpp>
#include <benzin/core/command_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/query_heap.hpp>
#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    static uint8_t CalcBeginTimestampIndex(uint8_t readbackIndex)
    {
        return readbackIndex * 2;
    }

    static uint8_t CalcEndTimestampIndex(uint8_t readbackIndex)
    {
        return readbackIndex * 2 + 1;
    }

    // GpuProfiler

    GpuProfiler::GpuProfiler(Device& device)
        : m_InverseFrequency{ 1.0 / (double)device.GetGraphicsCommandQueue().GetTimestampFrequency() }
    {
        MakeUniquePtr(m_TimestampQueryHeap, device, QueryHeapCreation
        {
            .DebugName = "GpuProfiler_Timestamp",
            .Type = QueryHeapType::Timestamp,
            .Count = ms_MaxTimestampCount,
        });

        MakeUniquePtr(m_ReadbackBuffer, device, BufferCreation
        {
            .DebugName = "GpuProfiler_ReadbackBuffer",
            .MemoryType = ResourceMemoryType::Readback,
            .ElementSize = sizeof(uint64_t) * ms_MaxTimestampCount,
            .ElementCount = CommandLineArgs::GetU32("FrameInFlightCount") + 1,
        });
    }

    GpuProfiler::~GpuProfiler() = default;

    std::span<const ProfileEvent> GpuProfiler::GetSortedEvents() const
    {
        return m_SortedEvents;
    }

    void GpuProfiler::BeginFrame(const Device& device)
    {
        m_ProfiledTimestamps.reset();
        m_SortCounter = 0;

        const auto cpuFrameIndex = device.GetCpuFrameIndex();
        const auto readbackLatency = m_ReadbackBuffer->GetElementCount();

        m_ResolveReadbackBufferOffset = (cpuFrameIndex % readbackLatency) * m_ReadbackBuffer->GetElementSize();
        m_CopyReadbackBufferOffset = ((cpuFrameIndex + 1) % readbackLatency) * m_ReadbackBuffer->GetElementSize();
    }

    void GpuProfiler::EndFrame()
    {
        GetTimestampsFromReadbackBuffer();
    }

    uint8_t GpuProfiler::AllocateEvent(std::string_view name)
    {
        BenzinEnsure(m_EventInfos.size() < ms_MaxEventCount);

        if (!m_PrevEventName.empty())
        {
            auto& prevEvent = m_EventInfos[m_PrevEventName];
            prevEvent.IsParent = prevEvent.Depth < m_CurrentDepth;
        }

        const auto readbackIndex = (uint8_t)m_EventInfos.size();
        auto&& [it, _] = m_EventInfos.try_emplace(name, readbackIndex, m_CurrentDepth);

        auto& eventInfo = (*it).second;
        eventInfo.SortIndex = std::max(eventInfo.SortIndex, m_SortCounter++);

        m_PrevEventName = name;

        return eventInfo.ReadbackIndex;
    }

    uint8_t GpuProfiler::GetBeginTimestampIndex(uint8_t eventIndex)
    {
        const auto index = CalcBeginTimestampIndex(eventIndex);

        m_ProfiledTimestamps[index] = true;
        m_CurrentDepth++;

        return index;
    }

    uint8_t GpuProfiler::GetEndTimestampIndex(uint8_t eventIndex)
    {
        const auto index = CalcEndTimestampIndex(eventIndex);

        m_ProfiledTimestamps[index] = true;
        m_CurrentDepth--;

        return index;
    }

    void GpuProfiler::ForceProfileUnprofiledTimestamps(const UnprofiledTimestampCallback& callback)
    {
        for (uint8_t i = 0; i < ms_MaxTimestampCount; ++i)
        {
            if (!m_ProfiledTimestamps[i])
            {
                callback(i);
            }
        }
    }

    void GpuProfiler::GetTimestampsFromReadbackBuffer()
    {
        const D3D12_RANGE d3d12ReadbackRange
        {
            .Begin = m_CopyReadbackBufferOffset,
            .End = m_CopyReadbackBufferOffset + m_ReadbackBuffer->GetElementSize(),
        };

        uint64_t* timestamps = nullptr;
        BenzinEnsure(m_ReadbackBuffer->GetD3D12Resource()->Map(0, &d3d12ReadbackRange, reinterpret_cast<void**>(&timestamps)));
        BenzinExecuteOnScopeExit([this] { m_ReadbackBuffer->GetD3D12Resource()->Unmap(0, nullptr); });

        const bool isNeedResize = m_SortedEvents.size() != m_EventInfos.size();
        if (isNeedResize)
        {
            m_SortedEvents.resize(m_EventInfos.size());
        }

        for (auto& [name, eventInfo] : m_EventInfos)
        {
            auto& sortEvent = m_SortedEvents[eventInfo.SortIndex];

            if (isNeedResize)
            {
                sortEvent.Name = name;
                sortEvent.Depth = eventInfo.Depth;
                sortEvent.IsParent = eventInfo.IsParent;
            }

            const auto beginTimestampIndex = CalcBeginTimestampIndex(eventInfo.ReadbackIndex);
            const auto endTimestampIndex = CalcEndTimestampIndex(eventInfo.ReadbackIndex);

            if (!m_ProfiledTimestamps[beginTimestampIndex] || !m_ProfiledTimestamps[endTimestampIndex])
            {
                sortEvent.Us = std::chrono::microseconds::zero();
            }
            else
            {
                const auto beginTimestamp = timestamps[beginTimestampIndex];
                const auto endTimestamp = timestamps[endTimestampIndex];

                const std::chrono::duration<double> diff{ (endTimestamp - beginTimestamp) * m_InverseFrequency};
                sortEvent.Us = std::chrono::round<std::chrono::microseconds>(diff);
            }
        }
    }

    // ScopedGpuProfileEvent

    ScopedGpuProfileEvent::ScopedGpuProfileEvent(GpuProfiler& gpuProfiler, GraphicsCommandList& commandList, uint8_t eventIndex)
        : m_GpuProfiler{ gpuProfiler }
        , m_CommandList{ commandList }
        , m_EventIndex{ eventIndex }
    {
        m_CommandList.SetTimestamp(m_GpuProfiler.GetTimestampQueryHeap(), m_GpuProfiler.GetBeginTimestampIndex(m_EventIndex));
    }

    ScopedGpuProfileEvent::~ScopedGpuProfileEvent()
    {
        m_CommandList.SetTimestamp(m_GpuProfiler.GetTimestampQueryHeap(), m_GpuProfiler.GetEndTimestampIndex(m_EventIndex));
    }

}
