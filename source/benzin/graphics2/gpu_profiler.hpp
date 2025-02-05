#pragma once

#include <benzin/core/index_allocator.hpp>

namespace benzin
{

    class Buffer;
    class Device;
    class GraphicsCommandList;
    class QueryHeap;

    struct ProfileEvent;

    class GpuProfiler
    {
    public:
        friend class ScopedGpuProfileEvent;

        using UnprofiledTimestampCallback = std::function<void(uint8_t timestampIndex)>;

        explicit GpuProfiler(Device& device);
        ~GpuProfiler();

    public:
        const auto& GetTimestampQueryHeap() const { return *m_TimestampQueryHeap; }
        const auto& GetReadbackBuffer() const { return *m_ReadbackBuffer; }

        auto GetResolveReadbackBufferOffset() const { return m_ResolveFrameData->ReadbackBufferOffset; }

        std::span<const ProfileEvent> GetSortedEvents() const;

    public:
        void BeginFrame(uint64_t cpuFrameIndex);
        void EndFrame();

        void ForceProfileUnprofiledTimestamps(const UnprofiledTimestampCallback& callback);

    private:
        uint64_t CalcEventHash(std::string_view name);
        std::pair<uint64_t, uint8_t> CreateOrUpdateEventInfo(std::string_view name);

        uint8_t GetBeginTimestampIndex(std::string_view name);
        uint8_t GetEndTimestampIndex();

        void GetTimestampsFromReadbackBuffer();

    private:
        static constexpr uint8_t ms_MaxTimestampCount = std::numeric_limits<uint8_t>::max();
        static constexpr uint8_t ms_MaxEventCount = ms_MaxTimestampCount / 2 - 1;

        struct EventInfo
        {
            const char* Name = nullptr;

            uint8_t Depth : 7 = 0;
            uint8_t IsParent : 1 = false;
            uint8_t SortIndex = 0;
        };

        struct FrameData
        {
            std::unordered_map<uint64_t, std::vector<uint8_t>> EventReadbackIndices;

            std::bitset<ms_MaxTimestampCount> ProfiledTimestamps;
            IndexAllocator ReadbackIndexAllocator{ ms_MaxTimestampCount };

            uint64_t ReadbackBufferOffset = 0;
            uint64_t* MappedTimestamps = nullptr;
        };

        double m_InverseFrequency = 0.0;

        std::unique_ptr<QueryHeap> m_TimestampQueryHeap;
        std::unique_ptr<Buffer> m_ReadbackBuffer;

        std::vector<FrameData> m_FrameData;
        FrameData* m_ResolveFrameData = nullptr;
        FrameData* m_CopyFrameData = nullptr;

        std::unordered_map<uint64_t, EventInfo> m_EventInfos;
        std::unordered_map<uint8_t, uint64_t> m_SortedEventHashes;
        std::stack<uint64_t, std::vector<uint64_t>> m_EventHashStack;
        std::vector<ProfileEvent> m_SortedEvents;

        uint8_t m_SortCounter = 0;
    };

    class ScopedGpuProfileEvent
    {
    public:
        ScopedGpuProfileEvent(GpuProfiler& gpuProfiler, GraphicsCommandList& commandList, std::string_view name);
        ~ScopedGpuProfileEvent();

    private:
        GpuProfiler& m_GpuProfiler;
        GraphicsCommandList& m_CommandList;
    };

}

#define BenzinGpuProfile(gpuProfiler, commandList, name) \
    BenzinGpuEvent(commandList, name); \
    const benzin::ScopedGpuProfileEvent BenzinUniqueVariableName(_scopedGpuProfileEvent){ gpuProfiler, commandList, name }
