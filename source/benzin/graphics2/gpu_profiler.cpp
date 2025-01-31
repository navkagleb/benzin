#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

#include <benzin/core/asserter.hpp>
#include <benzin/core/command_line_args.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/query_heap.hpp>
#include <benzin/utility/time_utils.hpp>

#include <benzin/core/logger.hpp>

namespace benzin
{

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

    uint32_t GpuProfiler::AllocateEvent(std::string_view name)
    {
        BenzinEnsure(m_EventInfos.size() < ms_MaxEventCount);

        if (!m_PrevEventName.empty())
        {
            auto& prevEvent = m_EventInfos[m_PrevEventName];
            prevEvent.IsParent = prevEvent.Depth < m_CurrentDepth;
        }

        auto&& [it, isNew] = m_EventInfos.try_emplace(name, (uint8_t)m_EventInfos.size(), m_CurrentDepth);

        auto& eventData = (*it).second;
        if (eventData.SortIndex < m_SortCounter)
        {
            eventData.SortIndex = m_SortCounter;
        }

        m_SortCounter++;
        m_PrevEventName = name;

        return eventData.ReadbackIndex;
    }

    uint32_t GpuProfiler::GetBeginTimestampIndex(uint32_t eventIndex)
    {
        const uint32_t index = eventIndex * 2;

        m_ProfiledTimestamps[index] = true;
        m_CurrentDepth++;

        return index;
    }

    uint32_t GpuProfiler::GetEndTimestampIndex(uint32_t eventIndex)
    {
        const uint32_t index = eventIndex * 2 + 1;

        m_ProfiledTimestamps[index] = true;
        m_CurrentDepth--;

        return index;
    }

    void GpuProfiler::ForceProfileUnprofiledTimestamps(const UnprofiledTimestampCallback& callback)
    {
        for (const uint32_t timestampIndex : std::views::iota(0u, ms_MaxTimestampCount))
        {
            if (!m_ProfiledTimestamps[timestampIndex])
            {
                callback(timestampIndex);
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

        m_SortedEvents.clear();
        m_SortedEvents.reserve(m_EventInfos.size());

        for (auto& [name, eventInfo] : m_EventInfos)
        {
            auto& readyEvent = m_SortedEvents.emplace_back();
            readyEvent.m_Name = name;
            readyEvent.m_Depth = eventInfo.Depth;
            readyEvent.m_IsParent = eventInfo.IsParent;
            readyEvent.m_SortIndex = eventInfo.SortIndex;

            const uint64_t beginTimestamp = timestamps[eventInfo.ReadbackIndex * 2];
            const uint64_t endTimeStamp = timestamps[eventInfo.ReadbackIndex * 2 + 1];

            if (endTimeStamp < beginTimestamp)
            {
                readyEvent.m_Us = std::chrono::microseconds::zero();
            }
            else
            {
                const std::chrono::duration<double> diff{ (endTimeStamp - beginTimestamp) * m_InverseFrequency };
                readyEvent.m_Us = std::chrono::round<std::chrono::microseconds>(diff);
            }
        }

        std::ranges::sort(m_SortedEvents, {}, &Event::m_SortIndex);
    }

    // ScopedGpuProfileEvent

    ScopedGpuProfileEvent::ScopedGpuProfileEvent(GpuProfiler& gpuProfiler, GraphicsCommandList& commandList, uint32_t eventIndex)
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
