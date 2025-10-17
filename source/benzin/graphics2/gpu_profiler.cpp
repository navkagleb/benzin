#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/query_heap.hpp>

namespace benzin
{

    static void ResetAccumulatedData(GpuProfileNode& node, uint32_t frameCount)
    {
        node.m_ImGuiDuration = std::exchange(node.m_AccumulatedDuration, {}) / frameCount;
        
        for (auto& child : node.m_Children)
        {
            ResetAccumulatedData(*child, frameCount);
        }
    }

    // GpuProfiler

    GpuProfiler::GpuProfiler(Device& device)
        : m_InvTimestampFrequency{ 1.0 / (double)device.GetGraphicsCmdQueue().GetTimestampFrequency() }
    {
        MakeUniquePtr(m_TimestampQueryHeap, device, QueryHeapCreation
        {
            .DebugName = "GpuProfiler::Timestamp",
            .Type = QueryHeapType::Timestamp,
            .Count = ms_MaxTimestampCount,
        });

        m_ReadbackBuffer = device.GetPersistentReadbackLinearAllocator().AllocateBuffer([](BufferCreation& creation)
        {
            creation.DebugName = "GpuProfiler::ReadbackBuffer";
            creation.ElementSizeInBytes = sizeof(uint64_t) * ms_MaxTimestampCount;
            creation.ElementCount = BENZIN_READBACK_LATENCY;
        });

        m_Root.m_Name = "Root";
    }

    GpuProfiler::~GpuProfiler() = default;

    const GpuProfileNode* GpuProfiler::GetRootNode() const
    {
        const auto& children = m_Root.m_Children;

        if (children.empty())
            return nullptr;

        BenzinAssert(children.size() == 1);
        return children.front().get();
    }

    void GpuProfiler::BeginFrame(uint64_t cpuFrameIndex)
    {
        BenzinProfile();

        if (m_NodeStack.empty())
        {
            m_NodeStack.emplace(&m_Root);
        }

        BenzinAssert(m_NodeStack.top() == &m_Root);

        m_ReadbackBuffer->MapReadbackData(
            m_ReadbackBuffer->GetElementSizeInBytes() * m_ReadIndex,
            m_ReadbackBuffer->GetElementSizeInBytes(),
            [this](const std::byte* mappedData)
            {
                const std::span<const uint64_t> timestamps = ToSpan((const uint64_t*)mappedData, ms_MaxTimestampCount);
                CopyNodeDurationRecursive(m_Root, timestamps);
            });

        m_WriteIndex = cpuFrameIndex % BENZIN_READBACK_LATENCY;
        m_ReadIndex = (cpuFrameIndex + 1) % BENZIN_READBACK_LATENCY;
    }

    void GpuProfiler::EndFrame()
    {
        BenzinProfile();

        BenzinAssert(m_NodeStack.top() == &m_Root);

        m_ReadbackIndexOffset = 0;
        m_ProfiledTimestamps.reset();
    }

    void GpuProfiler::ResolveTimestamps(ComputeCmdList& cmdList)
    {
        BenzinGpuProfile("GpuProfiler::ResolveTimestamps");

        for (uint32_t i = 0; i < ms_MaxTimestampCount; ++i)
        {
            if (!m_ProfiledTimestamps.test(i))
            {
                cmdList.SetTimestamp(*m_TimestampQueryHeap, i);
            }
        }

        cmdList.ResolveTimestamps(*m_TimestampQueryHeap, *m_ReadbackBuffer, m_ReadbackBuffer->GetElementSizeInBytes() * m_WriteIndex);
    }

    void GpuProfiler::ResetAccumulatedData(uint32_t frameCount)
    {
        benzin::ResetAccumulatedData(m_Root, frameCount);
    }

    void GpuProfiler::CopyNodeDurationRecursive(GpuProfileNode& node, std::span<const uint64_t> timestamps)
    {
        uint32_t& readbackIndex = node.m_ReadbackIndices[m_ReadIndex];
        if (readbackIndex == GpuProfileNode::ms_InvalidReadbackIndex)
        {
            node.m_Duration = {};
        }
        else
        {
            const auto beginTimestamp = timestamps[readbackIndex * 2];
            const auto endTimestamp = timestamps[readbackIndex * 2 + 1];

            const double durationInSec = (endTimestamp - beginTimestamp) * m_InvTimestampFrequency;
            const uint64_t durationInNs = (uint64_t)(durationInSec * 1000 * 1000 * 1000);

            node.m_Duration = GpuProfileNode::Duration{ durationInNs };
            readbackIndex = GpuProfileNode::ms_InvalidReadbackIndex;
        }

        node.m_AccumulatedDuration += std::exchange(node.m_Duration, {});
        node.m_CurrentChildOffset = 0;

        for (const auto& child : node.m_Children)
        {
            CopyNodeDurationRecursive(*child, timestamps);
        }
    }

    GpuProfileNode* GpuProfiler::GetOrCreateNode(std::string_view name)
    {
        if (m_NodeStack.empty())
            return nullptr;

        GpuProfileNode* parent = m_NodeStack.top();
        GpuProfileNode* node = parent->GetAndUpdateChild(name);

        BenzinAssert(node->m_ReadbackIndices[m_WriteIndex] == GpuProfileNode::ms_InvalidReadbackIndex);
        BenzinAssert(m_ReadbackIndexOffset < ms_MaxTimestampCount / 2);
        node->m_ReadbackIndices[m_WriteIndex] = m_ReadbackIndexOffset++;

        return node;
    }

    // ScopedGpuProfileEvent

    ScopedGpuProfileEvent::ScopedGpuProfileEvent(std::string_view name)
    {
        m_Node = ms_GpuProfiler->GetOrCreateNode(name);

        if (m_Node == nullptr)
            return;

        ms_GpuProfiler->m_NodeStack.push(m_Node);

        const uint32_t timestampIndex = m_Node->m_ReadbackIndices[ms_GpuProfiler->m_WriteIndex] * 2;

        ms_Device->GetGraphicsCmdQueue().GetCmdList().SetTimestamp(*ms_GpuProfiler->m_TimestampQueryHeap, timestampIndex);
        ms_GpuProfiler->m_ProfiledTimestamps.set(timestampIndex);
    }

    ScopedGpuProfileEvent::~ScopedGpuProfileEvent()
    {
        if (m_Node == nullptr)
            return;

        const uint32_t timestampIndex = m_Node->m_ReadbackIndices[ms_GpuProfiler->m_WriteIndex] * 2 + 1;

        ms_Device->GetGraphicsCmdQueue().GetCmdList().SetTimestamp(*ms_GpuProfiler->m_TimestampQueryHeap, timestampIndex);
        ms_GpuProfiler->m_ProfiledTimestamps.set(timestampIndex);

        BenzinAssert(ms_GpuProfiler->m_NodeStack.top() == m_Node);
        ms_GpuProfiler->m_NodeStack.pop();
    }

    void ScopedGpuProfileEvent::SetContext(Device& device, GpuProfiler& gpuProfiler)
    {
        ms_Device = &device;
        ms_GpuProfiler = &gpuProfiler;
    }

}
