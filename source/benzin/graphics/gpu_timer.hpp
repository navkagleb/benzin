#pragma once

#include "benzin/graphics/buffer.hpp"

namespace benzin
{

    class CommandList;
    class Device;

    struct GpuTimerCreation
    {
        CommandList& CommandList;

        uint64_t TimestampFrequency = 0; // Ticks per Second
        uint32_t TimerCount = 0;
    };

    class GpuTimer
    {
    public:
        BenzinDefineNonCopyable(GpuTimer);
        BenzinDefineNonMoveable(GpuTimer);

        GpuTimer(Device& device, const GpuTimerCreation& creation);
        ~GpuTimer();

    public:
        void BeginProfile(uint32_t timerIndex);
        void EndProfile(uint32_t timerIndex);

        std::chrono::microseconds GetElapsedTime(uint32_t timerIndex) const;

        void ResolveTimestamps(uint64_t cpuFrameIndex);

    private:
        void EndQuery(uint32_t timeStampIndex);
        void ForceProfileUnprofiledTimers();

    private:
        const float m_InverseFrequency = 0.0f;
        const uint32_t m_ReadbackLatency = g_InvalidIndex<uint32_t>;

        CommandList& m_ProfiledCommandList;

        ID3D12QueryHeap* m_D3D12TimestampQueryHeap = nullptr;
        Buffer m_ReadbackBuffer;

        std::vector<uint64_t> m_Timestamps;
        uint32_t m_ProfiledTimers = 0;
    };

    class GpuEventTracker
    {
    public:
        BenzinDefineNonConstructable(GpuEventTracker);

        static void BeginEvent(const CommandList& commandList, std::string_view eventName);
        static void EndEvent(const CommandList& commandList);
    };

} // namespace benzin

#define BenzinGrabGpuTimeOnScopeExit(gpuTimer, timerIndex) \
    (gpuTimer).BeginProfile(timerIndex); \
    BenzinExecuteOnScopeExit([&] { (gpuTimer).EndProfile(timerIndex); })

#define BenzinPushGpuEvent(commandList, eventName) \
    benzin::GpuEventTracker::BeginEvent(commandList, eventName); \
    BenzinExecuteOnScopeExit([&] { benzin::GpuEventTracker::EndEvent(commandList); })
