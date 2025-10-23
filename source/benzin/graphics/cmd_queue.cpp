#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/cmd_queue.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/descriptor_manager.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/fence.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

namespace benzin
{

    GraphicsCmdQueue::GraphicsCmdQueue(Device& device)
        : m_Device{ device }
        , m_CmdList{ device }
    {
        const D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc
        {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0,
        };

        BenzinD3D12Call(device.GetD3D12Device()->CreateCommandQueue(&d3d12CommandQueueDesc, IID_PPV_ARGS(&m_D3D12CommandQueue)));
        SetD3DObjectDebugName(m_D3D12CommandQueue, "GraphicsCmdQueue");

        for (uint32_t i = 0; i < BENZIN_FRAME_COUNT; ++i)
        {
            auto*& d3d12CommandAllocator = m_FrameContexts[i].m_D3D12CommandAllocator;

            BenzinD3D12Call(device.GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12CommandAllocator)));
            SetD3DObjectDebugName(d3d12CommandAllocator, std::format("GraphicsCommandAllocator{}", i));
        }

        MakeUniquePtr(m_FlushFence, device, FenceCreation
        {
            .DebugName = GetD3DObjectDebugName(m_D3D12CommandQueue) + "FlushFence",
            .InitialValue = m_FlushCount,
        });
    }

    GraphicsCmdQueue::~GraphicsCmdQueue()
    {
        for (auto& frameContext : m_FrameContexts)
        {
            SafeReleaseD3DObject(frameContext.m_D3D12CommandAllocator);
        }

        SafeReleaseD3DObject(m_D3D12CommandQueue);
    }

    GraphicsCmdList& GraphicsCmdQueue::GetCmdList(uint64_t uploadBufferSizeInBytes)
    {
        if (uploadBufferSizeInBytes != 0)
        {
            auto& uploadBuffers = m_FrameContexts[m_Device.GetActiveFrameIndex()].m_UploadBuffers;

            auto& uploadBuffer = uploadBuffers.emplace_back();
            MakeUniquePtr(uploadBuffer, m_Device, BufferCreation
            {
                .DebugName = std::format("UploadBuffer{}", uploadBuffers.size() - 1),
                .HeapType = GpuHeapType::Upload,
                .Type = BufferType::Byte,
                .ElementSizeInBytes = sizeof(std::byte),
                .ElementCount = uploadBufferSizeInBytes,
            });

            m_CmdList.SetUploadBuffer(*uploadBuffer);
        }

        return m_CmdList;
    }

    uint64_t GraphicsCmdQueue::GetTimestampFrequency() const
    {
        uint64_t frequency = 0;
        BenzinD3D12Call(m_D3D12CommandQueue->GetTimestampFrequency(&frequency));

        return frequency;
    }

    void GraphicsCmdQueue::ResetCmdList()
    {
        BenzinProfile();

        auto& frameContext = m_FrameContexts[m_Device.GetActiveFrameIndex()];

        auto* d3d12CommandAllocator = frameContext.m_D3D12CommandAllocator;
        BenzinD3D12Call(d3d12CommandAllocator->Reset());

        frameContext.m_UploadBuffers.clear();

        ID3D12GraphicsCommandList1* d3d12GraphicsCommandList = m_CmdList.GetD3D12GraphicsCommandList();
        BenzinD3D12Call(d3d12GraphicsCommandList->Reset(d3d12CommandAllocator, nullptr));

        ID3D12DescriptorHeap* const d3d12DescriptorHeaps[]
        {
            m_Device.GetDescriptorManager().GetD3D12GpuResourceDescriptorHeap(),
        };
        d3d12GraphicsCommandList->SetDescriptorHeaps((uint32_t)std::size(d3d12DescriptorHeaps), d3d12DescriptorHeaps);

        d3d12GraphicsCommandList->SetComputeRootSignature(m_Device.GetUnifiedRootSignature().GetD3D12RootSignature());
        d3d12GraphicsCommandList->SetGraphicsRootSignature(m_Device.GetUnifiedRootSignature().GetD3D12RootSignature());
    }

    void GraphicsCmdQueue::SubmitCmdList()
    {
        BenzinProfile();

        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_CmdList.GetD3D12GraphicsCommandList();
        BenzinD3D12Call(d3d12GraphicsCommandList->Close());

        ID3D12CommandList* const d3d12CommandLists[]{ d3d12GraphicsCommandList };
        m_D3D12CommandQueue->ExecuteCommandLists((uint32_t)std::size(d3d12CommandLists), d3d12CommandLists);
    }

    void GraphicsCmdQueue::Flush()
    {
        m_FlushCount++;

        BenzinTraceScopeTime("Flush Command queue {}. FlushCount: {}", GetD3DObjectDebugName(m_D3D12CommandQueue), m_FlushCount);

        SignalFence(*m_FlushFence, m_FlushCount);
        m_FlushFence->StopCurrentThreadBeforeGpuFinish(m_FlushCount);
    }

    void GraphicsCmdQueue::SignalFence(Fence& fence, uint64_t value)
    {
        BenzinD3D12Call(m_D3D12CommandQueue->Signal(fence.GetD3D12Fence(), value));
    }

}
