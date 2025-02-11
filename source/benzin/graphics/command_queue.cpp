#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/command_queue.hpp"

#include "benzin/core/command_line_args.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/descriptor_manager.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/fence.hpp"
#include "benzin/graphics/hr_assert.hpp"
#include "benzin/graphics/unified_root_signature.hpp"

namespace benzin
{

    GraphicsCommandQueue::GraphicsCommandQueue(Device& device)
        : m_Device{ device }
        , m_CommandList{ device }
    {
        const D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc
        {
            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0,
        };

        BenzinHrEnsure(device.GetD3D12Device()->CreateCommandQueue(&d3d12CommandQueueDesc, IID_PPV_ARGS(&m_D3D12CommandQueue)));
        SetDxObjectDebugName(m_D3D12CommandQueue, "GraphicsCommandQueue");

        m_FrameContexts.resize(CommandLineArgs::GetU32("FrameInFlightCount"));
        for (const auto [i, frameContext] : m_FrameContexts | std::views::enumerate)
        {
            auto*& d3d12CommandAllocator = frameContext.D3D12CommandAllocator;

            BenzinHrEnsure(device.GetD3D12Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12CommandAllocator)));
            SetDxObjectDebugName(d3d12CommandAllocator, std::format("GraphicsCommandAllocator{}", i));
        }

        MakeUniquePtr(m_FlushFence, device, FenceCreation
        {
            .DebugName = GetDxObjectDebugName(m_D3D12CommandQueue) + "FlushFence",
            .InitialValue = m_FlushCount,
        });
    }

    GraphicsCommandQueue::~GraphicsCommandQueue()
    {
        for (auto& frameContext : m_FrameContexts)
        {
            BenzinSafeDxObjectRelease(frameContext.D3D12CommandAllocator);
        }

        m_FrameContexts.clear();

        BenzinSafeDxObjectRelease(m_D3D12CommandQueue);
    }

    GraphicsCommandList& GraphicsCommandQueue::GetCommandList(Bytes32 uploadBufferSize)
    {
        if (uploadBufferSize != 0)
        {
            auto& uploadBuffers = m_FrameContexts[m_Device.GetActiveFrameIndex()].UploadBuffers;

            auto& uploadBuffer = uploadBuffers.emplace_back();
            MakeUniquePtr(uploadBuffer, m_Device, BufferCreation
            {
                .DebugName = std::format("UploadBuffer{}", uploadBuffers.size() - 1),
                .MemoryType = ResourceMemoryType::Upload,
                .Type = BufferType::Byte,
                .ElementSize = sizeof(std::byte),
                .ElementCount = uploadBufferSize,
            });

            m_CommandList.SetUploadBuffer(*uploadBuffer);
        }

        return m_CommandList;
    }

    uint64_t GraphicsCommandQueue::GetTimestampFrequency() const
    {
        uint64_t frequency = 0;
        BenzinHrEnsure(m_D3D12CommandQueue->GetTimestampFrequency(&frequency));

        return frequency;
    }

    void GraphicsCommandQueue::ResetCommandList()
    {
        auto& frameContext = m_FrameContexts[m_Device.GetActiveFrameIndex()];

        auto* d3d12CommandAllocator = frameContext.D3D12CommandAllocator;
        BenzinHrEnsure(d3d12CommandAllocator->Reset());

        frameContext.UploadBuffers.clear();

        ID3D12GraphicsCommandList1* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        BenzinHrEnsure(d3d12GraphicsCommandList->Reset(d3d12CommandAllocator, nullptr));

        ID3D12DescriptorHeap* const d3d12DescriptorHeaps[]
        {
            m_Device.GetDescriptorManager().GetD3D12GpuResourceDescriptorHeap(),
        };
        d3d12GraphicsCommandList->SetDescriptorHeaps((uint32_t)std::size(d3d12DescriptorHeaps), d3d12DescriptorHeaps);

        d3d12GraphicsCommandList->SetComputeRootSignature(m_Device.GetUnifiedRootSignature().GetD3D12RootSignature());
        d3d12GraphicsCommandList->SetGraphicsRootSignature(m_Device.GetUnifiedRootSignature().GetD3D12RootSignature());
    }

    void GraphicsCommandQueue::SubmitCommandList()
    {
        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        BenzinHrEnsure(d3d12GraphicsCommandList->Close());

        ID3D12CommandList* const d3d12CommandLists[]{ d3d12GraphicsCommandList };
        m_D3D12CommandQueue->ExecuteCommandLists((uint32_t)std::size(d3d12CommandLists), d3d12CommandLists);
    }

    void GraphicsCommandQueue::Flush()
    {
        m_FlushCount++;

        BenzinLogTimeOnScopeExit("Flush Command queue {}. FlushCount: {}", GetDxObjectDebugName(m_D3D12CommandQueue), m_FlushCount);

        SignalFence(*m_FlushFence, m_FlushCount);
        m_FlushFence->StopCurrentThreadBeforeGpuFinish(m_FlushCount);
    }

    void GraphicsCommandQueue::SignalFence(Fence& fence, uint64_t value)
    {
        BenzinHrEnsure(m_D3D12CommandQueue->Signal(fence.GetD3D12Fence(), value));
    }

} // namespace benzin
