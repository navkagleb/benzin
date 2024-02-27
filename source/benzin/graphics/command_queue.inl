#include "benzin/graphics/device.hpp"
#include "benzin/graphics/fence.hpp"

#include "benzin/core/asserter.hpp"

namespace benzin
{

    namespace
    {

        template <typename T>
        constexpr CommandListType GetCommandListType()
        {
            if constexpr (std::is_same_v<T, CopyCommandList>)
            {
                return CommandListType::Copy;
            }
            else if constexpr (std::is_same_v<T, ComputeCommandList>)
            {
                return CommandListType::Compute;
            }
            else if constexpr (std::is_same_v<T, GraphicsCommandList>)
            {
                return CommandListType::Direct;
            }
            else
            {
                static_assert(g_DependentFalse<T>);
            }
        }

    } // anonymous namespace

    template <std::derived_from<CommandList> CommandListT>
    CommandQueue<CommandListT>::CommandQueue(Device& device, size_t commandAllocatorCount)
        : m_Device{ device }
        , m_CommandList{ device }
    {
        const CommandListType commandListType = GetCommandListType<CommandListT>();
        const char* commandListTypeName = magic_enum::enum_name(commandListType).data();

        const D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc
        {
            .Type = (D3D12_COMMAND_LIST_TYPE)commandListType,
            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0,
        };

        BenzinEnsure(device.GetD3D12Device()->CreateCommandQueue(&d3d12CommandQueueDesc, IID_PPV_ARGS(&m_D3D12CommandQueue)));
        SetD3D12ObjectDebugName(m_D3D12CommandQueue, commandListTypeName + "CommandQueue"s);

        m_D3D12CommandAllocators.resize(commandAllocatorCount, nullptr);
        for (const auto [i, d3d12CommandAllocator] : m_D3D12CommandAllocators | std::views::enumerate)
        {
            BenzinEnsure(device.GetD3D12Device()->CreateCommandAllocator((D3D12_COMMAND_LIST_TYPE)commandListType, IID_PPV_ARGS(&d3d12CommandAllocator)));
            SetD3D12ObjectDebugName(d3d12CommandAllocator, commandListTypeName + "CommandAllocator"s, (uint32_t)i);
        }

        m_FlushFence = std::make_unique<StalledFence>(device, GetD3D12ObjectDebugName(m_D3D12CommandQueue) + "FlushFence");
    }

    template <std::derived_from<CommandList> CommandListT>
    CommandQueue<CommandListT>::~CommandQueue()
    {
        Flush();

        for (auto& d3d12CommandAllocator : m_D3D12CommandAllocators)
        {
            SafeUnknownRelease(d3d12CommandAllocator);
        }

        SafeUnknownRelease(m_D3D12CommandQueue);
    }

    template <std::derived_from<CommandList> CommandListT>
    uint64_t CommandQueue<CommandListT>::GetTimestampFrequency() const
    {
        BenzinAssert(m_D3D12CommandQueue);

        uint64_t frequency = 0;
        BenzinEnsure(m_D3D12CommandQueue->GetTimestampFrequency(&frequency));

        return frequency;
    }

    template <std::derived_from<CommandList> CommandListT>
    void CommandQueue<CommandListT>::ResetCommandList(uint32_t commandAllocatorIndex)
    {
        BenzinAssert(commandAllocatorIndex < m_D3D12CommandAllocators.size());

        ID3D12CommandAllocator* d3d12CommandAllocator = m_D3D12CommandAllocators[commandAllocatorIndex];
        BenzinEnsure(d3d12CommandAllocator->Reset());

        ID3D12GraphicsCommandList1* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        BenzinEnsure(d3d12GraphicsCommandList->Reset(d3d12CommandAllocator, nullptr));

        InitCommandList();
        m_IsCommandListExecuted = false;
    }

    template <std::derived_from<CommandList> CommandListT>
    void CommandQueue<CommandListT>::SumbitCommandList()
    {
        if (m_IsCommandListExecuted)
        {
            return;
        }

        ID3D12GraphicsCommandList* d3d12GraphicsCommandList = m_CommandList.GetD3D12GraphicsCommandList();
        BenzinEnsure(d3d12GraphicsCommandList->Close());

        ID3D12CommandList* const d3d12CommandLists[]{ d3d12GraphicsCommandList };
        m_D3D12CommandQueue->ExecuteCommandLists((uint32_t)std::size(d3d12CommandLists), d3d12CommandLists);

        m_IsCommandListExecuted = true;
    }

    template <std::derived_from<CommandList> CommandListT>
    void CommandQueue<CommandListT>::Flush()
    {
        m_FlushCount++;

        BenzinLogTimeOnScopeExit("Flush Command queue {}. Probably Need to wait. FlushCount: {}", GetD3D12ObjectDebugName(m_D3D12CommandQueue), m_FlushCount);

        SignalFence(*m_FlushFence, m_FlushCount);
        m_FlushFence->StallCurrentThreadUntilGPUCompletion(m_FlushCount);
    }

    template <std::derived_from<CommandList> CommandListT>
    void CommandQueue<CommandListT>::SignalFence(StalledFence& fence, uint64_t value)
    {
        BenzinEnsure(m_D3D12CommandQueue->Signal(fence.GetD3D12Fence(), value));
    }

} // namespace benzin
