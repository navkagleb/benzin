#pragma once

#include "benzin/graphics/command_queue.hpp"

namespace benzin
{

    class Buffer;
    class CommandList;
    class Device;

    class GPUTimer
    {
    public:
        BenzinDefineNonCopyable(GPUTimer);
        BenzinDefineNonMoveable(GPUTimer);

    private:
        static const uint32_t ms_BufferFrameCount = config::g_BackBufferCount + 1;

    public:
        GPUTimer(Device& device, const auto& commandQueue, size_t timerCount)
            : m_TimerSlotCount{ static_cast<uint32_t>(timerCount) * 2 }
            , m_InverseFrequency{ 1.0f / static_cast<float>(commandQueue.GetTimestampFrequency()) } // Counts per Second
        {
            CreateResources(device);

            m_TimeStamps.resize(m_TimerSlotCount);
        }

        ~GPUTimer();

    public:
        void OnEndFrame(CommandList& commandList);

        void Start(CommandList& commandList, uint32_t timerIndex = 0);
        void Stop(CommandList& commandList, uint32_t timerIndex = 0);

        void Start(CommandList& commandList, Enum auto timerIndex) { Start(commandList, magic_enum::enum_integer(timerIndex)); }
        void Stop(CommandList& commandList, Enum auto timerIndex) { Stop(commandList, magic_enum::enum_integer(timerIndex)); }

        MilliSeconds GetElapsedTime(uint32_t timerIndex) const;
        MilliSeconds GetElapsedTime(Enum auto timerIndex) const { return GetElapsedTime(magic_enum::enum_integer(timerIndex)); }

    private:
        static uint32_t GetTimerSlotIndexOnStart(uint32_t timerIndex) { return timerIndex * 2; }
        static uint32_t GetTimerSlotIndexOnStop(uint32_t timerIndex) { return timerIndex * 2 + 1; }

        void CreateResources(Device& device);

    private:
        const uint32_t m_TimerSlotCount = 0;
        const float m_InverseFrequency = 0.0f;

        ID3D12QueryHeap* m_D3D12TimestampQueryHeap = nullptr;
        std::unique_ptr<Buffer> m_Buffer;

        std::vector<uint64_t> m_TimeStamps;
    };

} // namespace benzin

#define BenzinGPUTimerScopeMeasurement(gpuTimer, commandList) \
    gpuTimer.Start(commandList); \
    BenzinExecuteOnScopeExit([&] { gpuTimer.Stop(commandList); })

#define BenzinGPUTimerSlotScopeMeasurement(gpuTimer, commandList, timerIndex) \
    (gpuTimer).Start(commandList, timerIndex); \
    BenzinExecuteOnScopeExit([&] { (gpuTimer).Stop(commandList, timerIndex); })
