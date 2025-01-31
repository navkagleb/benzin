#pragma once

namespace benzin
{

    class Buffer;
    class Device;
    class GraphicsCommandList;
    class QueryHeap;

    class GpuProfiler
    {
    public:
        class Event
        {
        public:
            friend class GpuProfiler;

            auto GetName() const { return m_Name; }
            auto GetUs() const { return m_Us; }
            auto GetDepth() const { return m_Depth; }
            auto IsParent() const { return m_IsParent; }

        private:
            std::string_view m_Name;
            std::chrono::microseconds m_Us = std::chrono::microseconds::zero();
            uint8_t m_Depth : 7 = 0;
            uint8_t m_IsParent : 1 = false;
            uint8_t m_SortIndex = 0;
        };

        using UnprofiledTimestampCallback = std::function<void(uint32_t timestampIndex)>;

        explicit GpuProfiler(Device& device);
        ~GpuProfiler();

    public:
        const auto& GetTimestampQueryHeap() const { return *m_TimestampQueryHeap; }
        const auto& GetReadbackBuffer() const { return *m_ReadbackBuffer; }

        auto GetResolveReadbackBufferOffset() const { return m_ResolveReadbackBufferOffset; }

        auto GetSortedEvents() const { return std::span<const Event>{ m_SortedEvents }; }

    public:
        void BeginFrame(const Device& device);
        void EndFrame();

        uint32_t AllocateEvent(std::string_view name);

        uint32_t GetBeginTimestampIndex(uint32_t eventIndex);
        uint32_t GetEndTimestampIndex(uint32_t eventIndex);

        void ForceProfileUnprofiledTimestamps(const UnprofiledTimestampCallback& callback);

    private:
        void GetTimestampsFromReadbackBuffer();

    private:
        struct EventInfo
        {
            uint8_t ReadbackIndex = 0;
            uint8_t Depth : 7 = 0;
            uint8_t IsParent : 1 = false;
            uint8_t SortIndex = 0;
        };

        static constexpr uint32_t ms_MaxTimestampCount = std::numeric_limits<uint8_t>::max();
        static constexpr uint32_t ms_MaxEventCount = ms_MaxTimestampCount / 2;

        double m_InverseFrequency = 0.0;

        std::unique_ptr<QueryHeap> m_TimestampQueryHeap;
        std::unique_ptr<Buffer> m_ReadbackBuffer;

        uint64_t m_ResolveReadbackBufferOffset = 0;
        uint64_t m_CopyReadbackBufferOffset = 0;

        std::bitset<ms_MaxTimestampCount> m_ProfiledTimestamps;

        std::unordered_map<std::string_view, EventInfo> m_EventInfos;
        std::vector<Event> m_SortedEvents;

        uint8_t m_SortCounter = 0;
        uint8_t m_CurrentDepth = 0;

        std::string_view m_PrevEventName;
    };

    class ScopedGpuProfileEvent
    {
    public:
        ScopedGpuProfileEvent(GpuProfiler& gpuProfiler, GraphicsCommandList& commandList, uint32_t eventIndex);
        ~ScopedGpuProfileEvent();

    private:
        GpuProfiler& m_GpuProfiler;
        GraphicsCommandList& m_CommandList;

        const uint32_t m_EventIndex;
    };

}

#define BenzinGpuProfile(gpuProfiler, commandList, name) \
    const uint32_t BenzinUniqueVariableName(_gpuProfileEventIndex) = (gpuProfiler).AllocateEvent(name); \
    const benzin::ScopedGpuProfileEvent BenzinUniqueVariableName(_scopedGpuProfileEvent){ gpuProfiler, commandList, BenzinUniqueVariableName(_gpuProfileEventIndex) }
