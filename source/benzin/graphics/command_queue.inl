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
            else if constexpr (std::is_same_v<T, ComputeCommandList>)
            {
                return D3D12_COMMAND_LIST_TYPE_COMPUTE;
            }
            else if constexpr (std::is_same_v<T, GraphicsCommandList>)
            {
                return D3D12_COMMAND_LIST_TYPE_DIRECT;
            }
            else
            {
                static_assert(false);
            }
        }

        constexpr auto GetCommandListTypeStrings()
        {
            magic_enum::containers::array<D3D12_COMMAND_LIST_TYPE, std::string_view> commandQueueDebugNames;
            commandQueueDebugNames[D3D12_COMMAND_LIST_TYPE_COPY] = "Copy";
            commandQueueDebugNames[D3D12_COMMAND_LIST_TYPE_COMPUTE] = "Compute";
            commandQueueDebugNames[D3D12_COMMAND_LIST_TYPE_DIRECT] = "Direct";

            return commandQueueDebugNames;
        }
        
        constexpr auto g_CommandListTypeStrings = GetCommandListTypeStrings();

    } // anonymous namespace

    template <typename T>
    CommandQueue<T>::CommandQueue(Device& device, size_t commandAllocatorCount)
        : m_Device{ device }
        , m_FlushFence{ device }
        , m_CommandList{ device }
    {
        const D3D12_COMMAND_LIST_TYPE d3d12CommandListType = GetD3D12CommandListType<T>();

        const D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc
        {
            .Type = d3d12CommandListType,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0,
        };

        BenzinAssert(device.GetD3D12Device()->CreateCommandQueue(&d3d12CommandQueueDesc, IID_PPV_ARGS(&m_D3D12CommandQueue)));

        m_D3D12CommandAllocators.resize(commandAllocatorCount, nullptr);
        for (size_t i = 0; i < m_D3D12CommandAllocators.size(); ++i)
        {
            ID3D12CommandAllocator*& d3d12CommandAllocator = m_D3D12CommandAllocators[i];

            BenzinAssert(device.GetD3D12Device()->CreateCommandAllocator(
                d3d12CommandListType,
                IID_PPV_ARGS(&d3d12CommandAllocator)
            ));

            SetD3D12ObjectDebugName(d3d12CommandAllocator, fmt::format("{}CommandAllocator", g_CommandListTypeStrings[d3d12CommandListType]), static_cast<uint32_t>(i));
        }

        SetD3D12ObjectDebugName(m_D3D12CommandQueue, fmt::format("{}CommandQueue", g_CommandListTypeStrings[d3d12CommandListType]));
        SetD3D12ObjectDebugName(m_FlushFence.GetD3D12Fence(), fmt::format("{}_FlushFence", GetD3D12ObjectDebugName(m_D3D12CommandQueue)));
    }

    template <typename T>
    CommandQueue<T>::~CommandQueue()
    {
        Flush();

        for (auto& d3d12CommandAllocator : m_D3D12CommandAllocators)
        {
            SafeUnknownRelease(d3d12CommandAllocator);
        }

        SafeUnknownRelease(m_D3D12CommandQueue);
    }

    template <typename T>
    void CommandQueue<T>::ResetCommandList(uint32_t commandAllocatorIndex)
    {
        BenzinAssert(commandAllocatorIndex < m_D3D12CommandAllocators.size());

        ID3D12CommandAllocator* d3d12CommandAllocator = m_D3D12CommandAllocators[commandAllocatorIndex];
        BenzinAssert(d3d12CommandAllocator->Reset());

        ID3D12GraphicsCommandList1* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        BenzinAssert(d3d12GraphicsCommandList->Reset(d3d12CommandAllocator, nullptr));

        InitCommandList();
        m_IsCommandListExecuted = false;
        m_IsNeedFlush = true;
    }

    template <typename T>
    void CommandQueue<T>::ExecuteCommandList()
    {
        if (m_IsCommandListExecuted)
        {
            return;
        }

        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        BenzinAssert(d3d12GraphicsCommandList->Close());

        ID3D12CommandList* const d3d12CommandLists[]{ d3d12GraphicsCommandList };
        m_D3D12CommandQueue->ExecuteCommandLists(static_cast<UINT>(std::size(d3d12CommandLists)), d3d12CommandLists);

        m_IsCommandListExecuted = true;
    }

    template <typename T>
    void CommandQueue<T>::Flush()
    {
        if (!m_IsNeedFlush)
        {
            return;
        }

        const std::string debugName = GetD3D12ObjectDebugName(m_D3D12CommandQueue);

        m_FlushCount++;

        BenzinTrace("{}: Begin Flush {}", debugName, m_FlushCount);
        
        UpdateFenceValue(m_FlushFence, m_FlushCount);
        m_FlushFence.WaitForGPU(m_FlushCount);

        BenzinTrace("{}: End Flush {}", debugName, m_FlushCount);

        m_IsNeedFlush = false;
    }

    template <typename T>
    void CommandQueue<T>::UpdateFenceValue(Fence& fence, uint64_t value)
    {
        BenzinAssert(fence.GetD3D12Fence());

        BenzinAssert(m_D3D12CommandQueue->Signal(fence.GetD3D12Fence(), value));
    }

} // namespace benzin
