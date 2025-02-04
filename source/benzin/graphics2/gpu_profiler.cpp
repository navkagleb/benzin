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

        m_FrameData.resize(m_ReadbackBuffer->GetElementCount());
        for (uint32_t i = 0; i < m_FrameData.size(); ++i)
        {
            auto& frameData = m_FrameData[i];
            frameData.ReadbackBufferOffset = m_ReadbackBuffer->GetElementSize() * i;

            const D3D12_RANGE d3d12ReadbackRange
            {
                .Begin = frameData.ReadbackBufferOffset,
                .End = frameData.ReadbackBufferOffset + m_ReadbackBuffer->GetElementSize(),
            };

            BenzinEnsure(m_ReadbackBuffer->GetD3D12Resource()->Map(0, &d3d12ReadbackRange, reinterpret_cast<void**>(&frameData.MappedTimestamps)));
        }
    }

    GpuProfiler::~GpuProfiler()
    {
        m_ReadbackBuffer->GetD3D12Resource()->Unmap(0, nullptr);

        for (auto& frameData : m_FrameData)
        {
            for (const auto& [_, readbackIndices] : frameData.EventReadbackIndices)
            {
                frameData.ReadbackIndexAllocator.FreeIndices(ToConstSpan(readbackIndices));
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
        m_SortCounter = 0;

        const auto resolveIndex = cpuFrameIndex % m_ReadbackBuffer->GetElementCount();
        const auto copyIndex = (cpuFrameIndex + 1) % m_ReadbackBuffer->GetElementCount();

        m_ResolveFrameData = &m_FrameData[resolveIndex];
        m_CopyFrameData = &m_FrameData[copyIndex];

        m_ResolveFrameData->ProfiledTimestamps.reset();
    }

    void GpuProfiler::EndFrame()
    {
        GetTimestampsFromReadbackBuffer();
    }

    uint8_t GpuProfiler::AllocateEvent(std::string_view name)
    {
        BenzinEnsure(!name.empty());
        BenzinEnsure(m_EventInfos.size() < ms_MaxEventCount);

        const uint64_t hash = CalcEventHash(name);

        auto&& [it, _] = m_EventInfos.try_emplace(hash, name.data(), (uint8_t)m_EventHashStack.size());
        EventInfo& eventInfo = it->second;

        auto& readbackIndices = m_ResolveFrameData->EventReadbackIndices[hash];
        readbackIndices.push_back((uint8_t)m_ResolveFrameData->ReadbackIndexAllocator.AllocateIndex());

        BenzinAssert(m_EventInfos.size() == m_ResolveFrameData->EventReadbackIndices.size());

        if (readbackIndices.size() == 1)
        {
            eventInfo.SortIndex = std::max(eventInfo.SortIndex, m_SortCounter++);

            const uint64_t prevHash = std::exchange(m_SortedEventHashes[eventInfo.SortIndex], hash);
            if (prevHash != 0 && prevHash != hash)
            {
                for (auto& frameData : m_FrameData)
                {
                    frameData.ReadbackIndexAllocator.FreeIndices(ToConstSpan(frameData.EventReadbackIndices[prevHash]));
                    frameData.EventReadbackIndices.erase(prevHash);
                }

                m_EventInfos.erase(prevHash);
            }
        }

        m_EventHashStack.push(hash);

        return readbackIndices.back();
    }

    uint8_t GpuProfiler::GetBeginTimestampIndex(uint8_t readbackIndex)
    {
        const auto timestampIndex = CalcBeginTimestampIndex(readbackIndex);
        m_ResolveFrameData->ProfiledTimestamps[timestampIndex] = true;

        return timestampIndex;
    }

    uint8_t GpuProfiler::GetEndTimestampIndex(uint8_t readbackIndex)
    {
        m_EventHashStack.pop();

        const auto timestampIndex = CalcEndTimestampIndex(readbackIndex);
        m_ResolveFrameData->ProfiledTimestamps[timestampIndex] = true;

        return timestampIndex;
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

        auto& parentEventInfo = m_EventInfos[parentHash];
        parentEventInfo.IsParent = true;

        return HashCombine(parentHash, name);
    }

    void GpuProfiler::GetTimestampsFromReadbackBuffer()
    {
        BenzinProfile();

        const bool isNeedResize = m_SortedEvents.size() != m_EventInfos.size();
        if (isNeedResize)
        {
            m_SortedEvents.resize(m_EventInfos.size());
        }

        for (auto& [hash, eventInfo] : m_EventInfos)
        {
            auto& sortEvent = m_SortedEvents[eventInfo.SortIndex];
            sortEvent.Us = std::chrono::microseconds::zero();

            if (isNeedResize)
            {
                sortEvent.Name = eventInfo.Name;
                sortEvent.Depth = eventInfo.Depth;
                sortEvent.IsParent = eventInfo.IsParent;
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

                const auto beginTimestamp = m_CopyFrameData->MappedTimestamps[beginTimestampIndex];
                const auto endTimestamp = m_CopyFrameData->MappedTimestamps[endTimestampIndex];

                const std::chrono::duration<double> diff{ (endTimestamp - beginTimestamp) * m_InverseFrequency };
                sortEvent.Us += std::chrono::round<std::chrono::microseconds>(diff);

                m_CopyFrameData->ReadbackIndexAllocator.FreeIndex(readbackIndex);
            }

            readbackIndices.clear();
        }
    }

    // ScopedGpuProfileEvent

    ScopedGpuProfileEvent::ScopedGpuProfileEvent(GpuProfiler& gpuProfiler, GraphicsCommandList& commandList, uint8_t readbackIndex)
        : m_GpuProfiler{ gpuProfiler }
        , m_CommandList{ commandList }
        , m_ReadbackIndex{ readbackIndex }
    {
        m_CommandList.SetTimestamp(m_GpuProfiler.GetTimestampQueryHeap(), m_GpuProfiler.GetBeginTimestampIndex(m_ReadbackIndex));
    }

    ScopedGpuProfileEvent::~ScopedGpuProfileEvent()
    {
        m_CommandList.SetTimestamp(m_GpuProfiler.GetTimestampQueryHeap(), m_GpuProfiler.GetEndTimestampIndex(m_ReadbackIndex));
    }

}
