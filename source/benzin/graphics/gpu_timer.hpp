#pragma once

#include "benzin/graphics/buffer.hpp"

namespace benzin
{

    class Device;
    class GraphicsCommandList;

    class GpuTimer
    {
    public:
        static const uint32_t s_MaxGpuTimerCount;

    public:
        BenzinDefineNonCopyable(GpuTimer);
        BenzinDefineNonMoveable(GpuTimer);

        GpuTimer(Device& device, uint32_t timerCount);
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
        const uint32_t m_ReadbackLatency = g_InvalidUnsigned<uint32_t>;

        GraphicsCommandList& m_ProfiledCommandList;

        ID3D12QueryHeap* m_D3D12TimestampQueryHeap = nullptr;
        Buffer m_ReadbackBuffer;

        std::vector<uint64_t> m_Timestamps;
        uint32_t m_ProfiledTimers = 0;
    };

    class GpuEventTracker
    {
    public:
        BenzinDefineNonConstructable(GpuEventTracker);

        static void BeginEvent(const GraphicsCommandList& commandList, std::string_view eventName);
        static void EndEvent(const GraphicsCommandList& commandList);
    };

    class ScopedGpuGrabTimer
    {
    public:
        ScopedGpuGrabTimer(GpuTimer& gpuTimer, uint32_t timerIndex);
        ~ScopedGpuGrabTimer();

    private:
        GpuTimer& m_GpuTimer;
        uint32_t m_TimerIndex = g_InvalidUnsigned<uint32_t>;
    };

} // namespace benzin

#define BenzinPushGpuEvent(commandList, eventName) \
    benzin::GpuEventTracker::BeginEvent(commandList, eventName); \
    BenzinExecuteOnScopeExit([&] { benzin::GpuEventTracker::EndEvent(commandList); })
