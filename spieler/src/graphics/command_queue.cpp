#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/command_queue.hpp"

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/graphics_command_list.hpp"

namespace spieler
{

	CommandQueue::CommandQueue(Device& device)
        : m_Fence{ device }
    {
        const D3D12_COMMAND_QUEUE_DESC commandQueueDesc
        {
            .Type{ D3D12_COMMAND_LIST_TYPE_DIRECT },
            .Priority{ 0 },
            .Flags{ D3D12_COMMAND_QUEUE_FLAG_NONE },
            .NodeMask{ 0 }
        };

        SPIELER_ASSERT(SUCCEEDED(device.GetDX12Device()->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_DX12CommandQueue))));
	}

    CommandQueue::~CommandQueue()
    {
        if (m_DX12CommandQueue)
        {
            Flush();
        }
    }

    void CommandQueue::Submit(GraphicsCommandList& commandList)
    {
        ID3D12CommandList* dx12CommandLists[]{ commandList.GetDX12GraphicsCommandList() };
        m_DX12CommandQueue->ExecuteCommandLists(1, dx12CommandLists);
    }

    void CommandQueue::Flush()
    {
        m_Fence.Increment();

        SPIELER_ASSERT(SUCCEEDED(m_DX12CommandQueue->Signal(m_Fence.GetDX12Fence(), m_Fence.GetValue())));
        m_Fence.WaitForGPU();
    }

} // namespace spieler
