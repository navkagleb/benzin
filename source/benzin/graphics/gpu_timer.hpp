#pragma once

#include "benzin/graphics/buffer.hpp"

namespace benzin
{

    class CommandList;
    class Device;

    struct GPUTimerCreation
    {
        CommandList& CommandList;

        uint64_t TimestampFrequency = 0; // Ticks per Second
        uint32_t TimerCount = 0;
    };

    class GPUTimer
    {
    public:
        BenzinDefineNonCopyable(GPUTimer);
        BenzinDefineNonMoveable(GPUTimer);

    public:
        GPUTimer(Device& device, const GPUTimerCreation& creation);
        ~GPUTimer();

    public:
        void BeginProfile(uint32_t timerIndex);
        void EndProfile(uint32_t timerIndex);

        std::chrono::microseconds GetElapsedTime(uint32_t timerIndex) const;

        void ResolveTimestamps();

    private:
        void EndQuery(uint32_t timeStampIndex);

    private:
        const float m_InverseFrequency = 0.0f;

        ID3D12GraphicsCommandList* m_ProfiledD3D12GraphicsCommandList = nullptr;

        ID3D12QueryHeap* m_D3D12TimestampQueryHeap = nullptr;
        Buffer m_ReadbackBuffer;

        std::vector<uint64_t> m_Timestamps;
    };

} // namespace benzin

#define BenzinGrabGPUTimeOnScopeExit(gpuTimer, timerIndex) \
    (gpuTimer).BeginProfile(timerIndex); \
    BenzinExecuteOnScopeExit([&] { (gpuTimer).EndProfile(timerIndex); })
