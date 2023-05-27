namespace benzin
{

    namespace
    {

        template <typename T>
        D3D12_COMMAND_LIST_TYPE GetD3D12CommandListType()
        {
            if constexpr (std::is_same_v<T, CopyCommandList>)
            {
                return D3D12_COMMAND_LIST_TYPE_COPY;
            }

            if constexpr (std::is_same_v<T, ComputeCommandList>)
            {
                return D3D12_COMMAND_LIST_TYPE_COMPUTE;
            }

            if constexpr (std::is_same_v<T, GraphicsCommandList>)
            {
                return D3D12_COMMAND_LIST_TYPE_DIRECT;
            }

            BENZIN_ASSERT(false);
        }

    } // anonymous namespace

    template <typename T>
    CommandQueue<T>::CommandQueue(Device& device, size_t commandAllocatorCount)
        : m_Device{ device }
        , m_FlushFence{ device }
        , m_CommandList{ device }
    {
        m_FlushFence.SetDebugName("CommandQueueFlush");

        const D3D12_COMMAND_LIST_TYPE d3d12CommandListType = GetD3D12CommandListType<T>();

        const D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc
        {
            .Type{ d3d12CommandListType },
            .Priority{ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL },
            .Flags{ D3D12_COMMAND_QUEUE_FLAG_NONE },
            .NodeMask{ 0 }
        };

        BENZIN_HR_ASSERT(device.GetD3D12Device()->CreateCommandQueue(
            &d3d12CommandQueueDesc,
            IID_PPV_ARGS(&m_D3D12CommandQueue)
        ));

        m_D3D12CommandAllocators.resize(commandAllocatorCount, nullptr);
        for (ID3D12CommandAllocator*& d3d12CommandAllocator : m_D3D12CommandAllocators)
        {
            BENZIN_HR_ASSERT(device.GetD3D12Device()->CreateCommandAllocator(
                d3d12CommandListType,
                IID_PPV_ARGS(&d3d12CommandAllocator)
            ));
        }
    }

    template <typename T>
    CommandQueue<T>::~CommandQueue()
    {
        Flush();

        for (auto& d3d12CommandAllocator : m_D3D12CommandAllocators)
        {
            dx::SafeRelease(d3d12CommandAllocator);
        }

        dx::SafeRelease(m_D3D12CommandQueue);
    }

    template <typename T>
    void CommandQueue<T>::ResetCommandList(uint32_t commandAllocatorIndex)
    {
        BENZIN_ASSERT(commandAllocatorIndex < m_D3D12CommandAllocators.size());

        ID3D12CommandAllocator* d3d12CommandAllocator = m_D3D12CommandAllocators[commandAllocatorIndex];
        BENZIN_HR_ASSERT(d3d12CommandAllocator->Reset());

        ID3D12GraphicsCommandList1* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        BENZIN_HR_ASSERT(d3d12GraphicsCommandList->Reset(d3d12CommandAllocator, nullptr));

        InitCommandList();
    }

    template <typename T>
    void CommandQueue<T>::ExecuteCommandList()
    {
        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        BENZIN_HR_ASSERT(d3d12GraphicsCommandList->Close());

        ID3D12CommandList* const d3d12CommandLists[]{ d3d12GraphicsCommandList };
        m_D3D12CommandQueue->ExecuteCommandLists(static_cast<UINT>(std::size(d3d12CommandLists)), d3d12CommandLists);
    }

    template <typename T>
    void CommandQueue<T>::Flush()
    {
        UpdateFenceValue(m_FlushFence, ++m_FlushCount);
        m_FlushFence.WaitForGPU(m_FlushCount);
    }

    template <typename T>
    void CommandQueue<T>::FlushAndResetCommandList()
    {
        ExecuteCommandList();
        Flush();
    }

    template <typename T>
    void CommandQueue<T>::UpdateFenceValue(Fence& fence, uint64_t value)
    {
        BENZIN_ASSERT(fence.GetD3D12Fence());

        BENZIN_HR_ASSERT(m_D3D12CommandQueue->Signal(fence.GetD3D12Fence(), value));
    }

} // namespace benzin
