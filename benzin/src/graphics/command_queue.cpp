#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/command_queue.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/graphics_command_list.hpp"

namespace benzin
{

	CommandQueue::CommandQueue(Device& device, std::string_view debugName)
        : m_FlushFence{ device, "CommandQueueFlush" }
    {
        const D3D12_COMMAND_QUEUE_DESC commandQueueDesc
        {
            .Type{ D3D12_COMMAND_LIST_TYPE_DIRECT },
            .Priority{ 0 },
            .Flags{ D3D12_COMMAND_QUEUE_FLAG_NONE },
            .NodeMask{ 0 }
        };

        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_D3D12CommandQueue)));
        SetDebugName(debugName, true);

        // Command Allocators
        {
            const D3D12_COMMAND_LIST_TYPE commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;

            for (size_t i = 0; i < m_D3D12CommandAllocators.size(); ++i)
            {
                ID3D12CommandAllocator*& d3d12CommandAllocator = m_D3D12CommandAllocators[i];

                BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&d3d12CommandAllocator)));
                
                const std::string debugName = fmt::format("CommandAllocator[{}]", i);
                detail::SetD3D12ObjectDebutName(d3d12CommandAllocator, debugName, true);
            }
        }

        m_GraphicsCommandList = new GraphicsCommandList{ device, m_D3D12CommandAllocators[0], "CommandQueue" };
	}

    CommandQueue::~CommandQueue()
    {
        Flush();

        delete m_GraphicsCommandList;

        for (auto& d3d12CommandAllocator : m_D3D12CommandAllocators)
        {
            SafeReleaseD3D12Object(d3d12CommandAllocator);
        }

        SafeReleaseD3D12Object(m_D3D12CommandQueue);
    }

    void CommandQueue::ResetGraphicsCommandList(uint32_t commandAllocatorIndex)
    {
        BENZIN_ASSERT(commandAllocatorIndex < m_D3D12CommandAllocators.size());

        ID3D12CommandAllocator* d3d12CommandAllocator = m_D3D12CommandAllocators[commandAllocatorIndex];
        BENZIN_D3D12_ASSERT(d3d12CommandAllocator->Reset());

        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_GraphicsCommandList->GetD3D12GraphicsCommandList();
        ID3D12PipelineState* d3d12InitialPipelineState = nullptr;
        BENZIN_D3D12_ASSERT(d3d12GraphicsCommandList->Reset(d3d12CommandAllocator, d3d12InitialPipelineState));
    }

    void CommandQueue::ExecuteGraphicsCommandList()
    {
        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_GraphicsCommandList->GetD3D12GraphicsCommandList();
        BENZIN_D3D12_ASSERT(d3d12GraphicsCommandList->Close());

        ID3D12CommandList* const d3d12CommandLists[]{ m_GraphicsCommandList->GetD3D12GraphicsCommandList() };
        m_D3D12CommandQueue->ExecuteCommandLists(static_cast<UINT>(std::size(d3d12CommandLists)), d3d12CommandLists);
    }

    void CommandQueue::Flush()
    {
        m_FlushCount++;
        BENZIN_D3D12_ASSERT(m_D3D12CommandQueue->Signal(m_FlushFence.GetD3D12Fence(), m_FlushCount));

        m_FlushFence.WaitForGPU(m_FlushCount);
    }

} // namespace benzin
