#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
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
        : m_InverseFrequency{ 1.0 / (double)device.GetGraphicsCmdQueue().GetTimestampFrequency() }
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
            .ElementSizeInBytes = sizeof(uint64_t) * ms_MaxTimestampCount,
            .ElementCount = CmdLineArgs::GetReadbackLatency(),
        });

        m_FrameData.resize(CmdLineArgs::GetReadbackLatency());
        for (uint32_t i = 0; i < m_FrameData.size(); ++i)
        {
            auto& frameData = m_FrameData[i];
            frameData.ReadbackOffsetInBytes = m_ReadbackBuffer->GetElementSizeInBytes() * i;
        }

        m_SortedEvents.reserve(ms_MaxEventCount);
    }

    GpuProfiler::~GpuProfiler()
    {
        for (auto& frameData : m_FrameData)
        {
            for (const auto& [_, readbackIndices] : frameData.EventReadbackIndices)
            {
                frameData.ReadbackIndexAllocator.FreeIndices(ToSpan(readbackIndices));
            }

            frameData.EventReadbackIndices.clear();
        }

        m_FrameData.clear();
    }

    std::span<const ProfileEvent> GpuProfiler::GetSortedEvents() const
    {
        return m_SortedEvents;
    }

    void GpuProfiler::BeginFrame(uint64_t cpuFrameIndex)
    {
        BenzinProfile();

        if (m_CopyFrameData != nullptr)
        {
            m_ReadbackBuffer->MapReadbackData(m_CopyFrameData->ReadbackOffsetInBytes, m_ReadbackBuffer->GetElementSizeInBytes(), [this](const std::byte* mappedData)
            {
                const auto* mappedTimestamps = (const uint64_t*)mappedData;
                GetTimestampsFromReadbackBuffer(mappedTimestamps);
            });
        }

        m_SortCounter = 0;

        const auto resolveIndex = cpuFrameIndex % CmdLineArgs::GetReadbackLatency();
        const auto copyIndex = (cpuFrameIndex + 1) % CmdLineArgs::GetReadbackLatency();

        m_ResolveFrameData = &m_FrameData[resolveIndex];
        m_CopyFrameData = &m_FrameData[copyIndex];

        m_ResolveFrameData->ProfiledTimestamps.reset();
    }

    void GpuProfiler::ForceProfileUnprofiledTimestamps(const UnprofiledTimestampCallback& callback)
    {
        for (uint8_t i = 0; i < ms_MaxTimestampCount; ++i)
        {
            if (!m_ResolveFrameData->ProfiledTimestamps[i])
            {
                callback(i);
            }
        }
    }

    uint64_t GpuProfiler::CalcEventHash(std::string_view name)
    {
        if (m_EventHashStack.empty())
        {
            return std::hash<std::string_view>{}(name);
        }

        const uint64_t parentHash = m_EventHashStack.top();

        m_HashToEventInfo[parentHash].IsParent = true;;

        return HashCombine(parentHash, name);
    }

    std::pair<uint64_t, uint8_t> GpuProfiler::CreateOrUpdateEventInfo(std::string_view name)
    {
        BenzinEnsure(!name.empty());
        BenzinEnsure(m_HashToEventInfo.size() < ms_MaxEventCount);

        const uint64_t hash = CalcEventHash(name);

        auto&& [it, _] = m_HashToEventInfo.try_emplace(hash, name.data(), (uint8_t)m_EventHashStack.size());
        EventInfo& eventInfo = it->second;

        auto& readbackIndices = m_ResolveFrameData->EventReadbackIndices[hash];
        readbackIndices.push_back((uint8_t)m_ResolveFrameData->ReadbackIndexAllocator.AllocateIndex());

        BenzinAssert(m_HashToEventInfo.size() == m_ResolveFrameData->EventReadbackIndices.size());

        if (readbackIndices.size() == 1)
        {
            m_SortCounter = std::max(eventInfo.SortIndex, m_SortCounter);

            eventInfo.SortIndex = m_SortCounter++;

            const uint64_t prevHash = std::exchange(m_SortIndexToHash[eventInfo.SortIndex], hash);
            if (prevHash != 0 && prevHash != hash)
            {
                m_HashToEventInfo.erase(prevHash);

                for (auto& frameData : m_FrameData)
                {
                    frameData.ReadbackIndexAllocator.FreeIndices(ToSpan(frameData.EventReadbackIndices[prevHash]));
                    frameData.EventReadbackIndices.erase(prevHash);
                }
            }

            BenzinAssert(m_HashToEventInfo.size() == m_SortIndexToHash.size());
            BenzinAssert(m_ResolveFrameData->EventReadbackIndices.size() == m_SortIndexToHash.size());
        }

        return { hash, readbackIndices.back() };
    }

    uint8_t GpuProfiler::GetBeginTimestampIndex(std::string_view name)
    {
        const auto [hash, readbackIndex] = CreateOrUpdateEventInfo(name);
        m_EventHashStack.push(hash);

        const auto timestampIndex = CalcBeginTimestampIndex(readbackIndex);
        m_ResolveFrameData->ProfiledTimestamps[timestampIndex] = true;

        return timestampIndex;
    }

    uint8_t GpuProfiler::GetEndTimestampIndex()
    {
        const auto readbackIndex = m_ResolveFrameData->EventReadbackIndices[m_EventHashStack.top()].back();
        m_EventHashStack.pop();

        const auto timestampIndex = CalcEndTimestampIndex(readbackIndex);
        m_ResolveFrameData->ProfiledTimestamps[timestampIndex] = true;

        return timestampIndex;
    }

    void GpuProfiler::GetTimestampsFromReadbackBuffer(const uint64_t* mappedTimestamps)
    {
        const auto eventCount = m_HashToEventInfo.size();

        const bool isNeedResize = m_SortedEvents.size() != eventCount;
        if (isNeedResize)
        {
            m_SortedEvents.resize(eventCount);
        }

        for (auto& [hash, eventInfo] : m_HashToEventInfo)
        {
            auto& sortedEvent = m_SortedEvents[eventInfo.SortIndex];
            sortedEvent.Us = std::chrono::microseconds::zero();

            if (isNeedResize)
            {
                sortedEvent.Name = eventInfo.Name;
                sortedEvent.Depth = eventInfo.Depth;
                sortedEvent.IsParent = eventInfo.IsParent;
            }

            auto& readbackIndices = m_CopyFrameData->EventReadbackIndices[hash];
            for (const uint8_t readbackIndex : readbackIndices)
            {
                const auto beginTimestampIndex = CalcBeginTimestampIndex(readbackIndex);
                const auto endTimestampIndex = CalcEndTimestampIndex(readbackIndex);

                if (!m_CopyFrameData->ProfiledTimestamps[beginTimestampIndex] || !m_CopyFrameData->ProfiledTimestamps[endTimestampIndex])
                {
                    continue;
                }

                const auto beginTimestamp = mappedTimestamps[beginTimestampIndex];
                const auto endTimestamp = mappedTimestamps[endTimestampIndex];

                const std::chrono::duration<double> diff{ (endTimestamp - beginTimestamp) * m_InverseFrequency };
                sortedEvent.Us += std::chrono::round<std::chrono::microseconds>(diff);

                m_CopyFrameData->ReadbackIndexAllocator.FreeIndex(readbackIndex);
            }

            readbackIndices.clear();
        }
    }

    // ScopedGpuProfileEvent

    ScopedGpuProfileEvent::ScopedGpuProfileEvent(GpuProfiler& gpuProfiler, ComputeCmdList& cmdList, std::string_view name)
        : m_GpuProfiler{ gpuProfiler }
        , m_CmdList{ cmdList }
    {
        m_CmdList.SetTimestamp(m_GpuProfiler.GetTimestampQueryHeap(), m_GpuProfiler.GetBeginTimestampIndex(name));
    }

    ScopedGpuProfileEvent::~ScopedGpuProfileEvent()
    {
        m_CmdList.SetTimestamp(m_GpuProfiler.GetTimestampQueryHeap(), m_GpuProfiler.GetEndTimestampIndex());
    }

}
