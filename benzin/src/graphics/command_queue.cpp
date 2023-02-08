#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/command_queue.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/graphics_command_list.hpp"

namespace benzin
{

	CommandQueue::CommandQueue(Device& device, const char* debugName)
        : m_Fence{ device }
    {
        const D3D12_COMMAND_QUEUE_DESC commandQueueDesc
        {
            .Type{ D3D12_COMMAND_LIST_TYPE_DIRECT },
            .Priority{ 0 },
            .Flags{ D3D12_COMMAND_QUEUE_FLAG_NONE },
            .NodeMask{ 0 }
        };

        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_D3D12CommandQueue)));

        SetDebugName(debugName);
	}

    CommandQueue::~CommandQueue()
    {
        if (m_D3D12CommandQueue)
        {
            Flush();
        }

        SafeReleaseD3D12Object(m_D3D12CommandQueue);
    }

    void CommandQueue::Submit(GraphicsCommandList& commandList)
    {
        ID3D12CommandList* d3d12CommandLists[]{ commandList.GetD3D12GraphicsCommandList() };
        m_D3D12CommandQueue->ExecuteCommandLists(1, d3d12CommandLists);
    }

    void CommandQueue::Flush()
    {
        m_Fence.Increment();

        BENZIN_D3D12_ASSERT(m_D3D12CommandQueue->Signal(m_Fence.GetD3D12Fence(), m_Fence.GetValue()));
        m_Fence.WaitForGPU();
    }

} // namespace benzin
