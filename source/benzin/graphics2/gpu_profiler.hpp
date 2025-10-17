#pragma once

#include <benzin/core/profiler.hpp>

namespace benzin
{

    class Buffer;
    class Device;
    class ComputeCmdList;
    class QueryHeap;

    struct GpuProfileNode : ProfileNodeBase<GpuProfileNode, std::chrono::duration<uint64_t, std::nano>>
    {
        static constexpr uint32_t ms_InvalidReadbackIndex = std::numeric_limits<uint32_t>::max();

        uint32_t m_ReadbackIndices[BENZIN_READBACK_LATENCY];

        GpuProfileNode()
        {
            std::ranges::fill(m_ReadbackIndices, ms_InvalidReadbackIndex);
        }
    };

    class GpuProfiler
    {
    public:
        friend class ScopedGpuProfileEvent;

        explicit GpuProfiler(Device& device);
        ~GpuProfiler();

        const GpuProfileNode* GetRootNode() const;

        void BeginFrame(uint64_t cpuFrameIndex);
        void EndFrame();

        void ResolveTimestamps(ComputeCmdList& cmdList);
        void ResetAccumulatedData(uint32_t frameCount);

    private:
        GpuProfileNode* GetOrCreateNode(std::string_view name);

        void CopyNodeDurationRecursive(GpuProfileNode& node, std::span<const uint64_t> timestamps);

        static constexpr uint32_t ms_MaxTimestampCount = 255;
        static constexpr uint32_t ms_MaxEventCount = ms_MaxTimestampCount / 2;

        GpuProfileNode m_Root; // Fake root node
        std::stack<GpuProfileNode*> m_NodeStack;

        double m_InvTimestampFrequency = 0.0;

        std::unique_ptr<QueryHeap> m_TimestampQueryHeap;
        std::unique_ptr<Buffer> m_ReadbackBuffer;

        uint32_t m_WriteIndex = 1;
        uint32_t m_ReadIndex = 0;

        uint32_t m_ReadbackIndexOffset = 0;
        std::bitset<ms_MaxTimestampCount> m_ProfiledTimestamps;
    };

    class ScopedGpuProfileEvent
    {
    public:
        ScopedGpuProfileEvent(std::string_view name);
        ~ScopedGpuProfileEvent();

        static void SetContext(Device& device, GpuProfiler& gpuProfiler);

    private:
        static inline Device* ms_Device = nullptr;
        static inline GpuProfiler* ms_GpuProfiler = nullptr;

        GpuProfileNode* m_Node = nullptr;
    };

}

#define BenzinGpuProfile(name) \
    BenzinGpuEvent(name); \
    const benzin::ScopedGpuProfileEvent BenzinUniqueVariableName(_scopedGpuProfileEvent){ name }
