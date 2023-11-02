#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/gpu_timer.hpp"

#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_list.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    GPUTimer::~GPUTimer()
    {
        SafeUnknownRelease(m_D3D12TimestampQueryHeap);
    }

    void GPUTimer::OnEndFrame(CommandList& commandList)
    {
        static uint32_t currentFrameIndex = 0;

        const uint32_t bufferSizePerFrame = m_Buffer->GetNotAlignedSizeInBytes() / ms_BufferFrameCount;

        // Resolve query for the current frame
        const uint64_t bufferOffset = currentFrameIndex * bufferSizePerFrame;
        commandList.GetD3D12GraphicsCommandList()->ResolveQueryData(m_D3D12TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, m_TimerSlotCount, m_Buffer->GetD3D12Resource(), bufferOffset);

        // Grab read-back data for the queries from a finished frame 'config::g_BackBufferCount' ago
        const uint32_t readbackFrameIndex = (currentFrameIndex + 1) % ms_BufferFrameCount;
        const size_t readbackBufferOffset = readbackFrameIndex * bufferSizePerFrame;

        const D3D12_RANGE d3d12BufferDataRange
        {
            .Begin = readbackBufferOffset,
            .End = readbackBufferOffset + bufferSizePerFrame,
        };

        uint64_t* timeStampsData;
        BenzinAssert(m_Buffer->GetD3D12Resource()->Map(0, &d3d12BufferDataRange, reinterpret_cast<void**>(&timeStampsData)));
        BenzinExecuteOnScopeExit([this] { m_Buffer->GetD3D12Resource()->Unmap(0, nullptr); });

        memcpy(m_TimeStamps.data(), timeStampsData, bufferSizePerFrame);

        currentFrameIndex = readbackFrameIndex;
    }

    void GPUTimer::Start(CommandList& commandList, uint32_t timerIndex)
    {
        BenzinAssert(timerIndex < m_TimerSlotCount / 2);

        commandList.GetD3D12GraphicsCommandList()->EndQuery(m_D3D12TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, GetTimerSlotIndexOnStart(timerIndex));
    }

    void GPUTimer::Stop(CommandList& commandList, uint32_t timerIndex)
    {
        BenzinAssert(timerIndex < m_TimerSlotCount / 2);

        commandList.GetD3D12GraphicsCommandList()->EndQuery(m_D3D12TimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, GetTimerSlotIndexOnStop(timerIndex));
    }

    float GPUTimer::GetElapsedTimeInSeconds(uint32_t timerIndex) const
    {
        BenzinAssert(timerIndex < m_TimerSlotCount / 2);

        const auto start = m_TimeStamps[GetTimerSlotIndexOnStart(timerIndex)];
        const auto stop = m_TimeStamps[GetTimerSlotIndexOnStop(timerIndex)];

        if (stop < start)
            return 0.0;

        return static_cast<float>(stop - start) * m_InverseFrequency;
    }

    void GPUTimer::CreateResources(Device& device)
    {
        BenzinAssert(device.GetD3D12Device());

        const D3D12_QUERY_HEAP_DESC d3d12QueryHeapDesc
        {
            .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
            .Count = m_TimerSlotCount,
            .NodeMask = 0,
        };

        BenzinAssert(device.GetD3D12Device()->CreateQueryHeap(&d3d12QueryHeapDesc, IID_PPV_ARGS(&m_D3D12TimestampQueryHeap)));
        SetD3D12ObjectDebugName(m_D3D12TimestampQueryHeap, "GPUTimer_TimestampQueryHeap");

        m_Buffer = std::make_unique<Buffer>(device, BufferCreation
        {
            .DebugName = "GPUTimer_Buffer",
            .ElementSize = sizeof(uint64_t),
            .ElementCount = ms_BufferFrameCount * m_TimerSlotCount,
            .Flags = BufferFlag::ReadbackBuffer,
            .InitialState = ResourceState::CopyDestination,
        });
    }

} // namespace benzin
