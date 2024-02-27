#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/gpu_timer.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_list.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    GPUTimer::GPUTimer(Device& device, const GPUTimerCreation& creation)
        : m_InverseFrequency{ 1.0f / creation.TimestampFrequency }
        , m_ProfiledD3D12GraphicsCommandList{ creation.CommandList.GetD3D12GraphicsCommandList() }
        , m_ReadbackBuffer{ device }
    {
        m_Timestamps.resize(creation.TimerCount * 2);

        {
            const D3D12_QUERY_HEAP_DESC d3d12QueryHeapDesc
            {
                .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
                .Count = (uint32_t)m_Timestamps.size(),
                .NodeMask = 0,
            };

            BenzinAssert(device.GetD3D12Device()->CreateQueryHeap(&d3d12QueryHeapDesc, IID_PPV_ARGS(&m_D3D12TimestampQueryHeap)));
            SetD3D12ObjectDebugName(m_D3D12TimestampQueryHeap, "GPUTimer_TimestampQueryHeap");
        }

        m_ReadbackBuffer.Create(BufferCreation
        {
            .DebugName = "GPUTimer_ReadbackBuffer",
            .ElementSize = sizeof(uint64_t) * (uint32_t)m_Timestamps.size(),
            .ElementCount = CommandLineArgs::GetFrameInFlightCount() + 1,
            .Flags = BufferFlag::ReadbackBuffer,
            .InitialState = ResourceState::CopyDestination,
        });
    }

    GPUTimer::~GPUTimer()
    {
        SafeUnknownRelease(m_D3D12TimestampQueryHeap);
    }

    void GPUTimer::BeginProfile(uint32_t timerIndex)
    {
        BenzinAssert(timerIndex < m_Timestamps.size() / 2);
        EndQuery(timerIndex * 2);
    }

    void GPUTimer::EndProfile(uint32_t timerIndex)
    {
        BenzinAssert(timerIndex < m_Timestamps.size() / 2);
        EndQuery(timerIndex * 2 + 1);
    }

    std::chrono::microseconds GPUTimer::GetElapsedTime(uint32_t timerIndex) const
    {
        BenzinAssert(timerIndex < m_Timestamps.size() / 2);

        const auto startTimeStamp = m_Timestamps[timerIndex * 2];
        const auto endTimeStamp = m_Timestamps[timerIndex * 2 + 1];

        if (endTimeStamp < startTimeStamp)
        {
            return std::chrono::microseconds::zero();
        }

        return SecToUS((endTimeStamp - startTimeStamp) * m_InverseFrequency);
    }

    void GPUTimer::ResolveTimestamps()
    {
        static uint32_t currentFrameIndex = 0;

        const auto GetReadbackBufferOffsetForCurrentFrame = [&]
        {
            return currentFrameIndex * m_ReadbackBuffer.GetElementSize();
        };

        {
            // Resolve query for the current frame

            m_ProfiledD3D12GraphicsCommandList->ResolveQueryData(
                m_D3D12TimestampQueryHeap,
                D3D12_QUERY_TYPE_TIMESTAMP,
                0,
                (uint32_t)m_Timestamps.size(),
                m_ReadbackBuffer.GetD3D12Resource(),
                GetReadbackBufferOffsetForCurrentFrame()
            );
        }

        {
            // Grab readback data from a finished frame FrameInFlighCount ago

            currentFrameIndex = (currentFrameIndex + 1) % m_ReadbackBuffer.GetElementCount();

            const size_t readbackBufferOffset = GetReadbackBufferOffsetForCurrentFrame();

            const D3D12_RANGE d3d12ReadbackRange
            {
                .Begin = readbackBufferOffset,
                .End = readbackBufferOffset + m_ReadbackBuffer.GetElementSize(),
            };

            uint64_t* timestampData = nullptr;
            BenzinAssert(m_ReadbackBuffer.GetD3D12Resource()->Map(0, &d3d12ReadbackRange, reinterpret_cast<void**>(&timestampData)));
            BenzinExecuteOnScopeExit([this] { m_ReadbackBuffer.GetD3D12Resource()->Unmap(0, nullptr); });

            memcpy(m_Timestamps.data(), timestampData, m_ReadbackBuffer.GetElementSize());
        }
    }

    void GPUTimer::EndQuery(uint32_t timestampIndex)
    {
        m_ProfiledD3D12GraphicsCommandList->EndQuery(m_D3D12TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, timestampIndex);
    }

} // namespace benzin
