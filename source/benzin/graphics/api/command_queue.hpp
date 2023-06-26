#pragma once

#include "benzin/graphics/api/fence.hpp"
#include "benzin/graphics/api/command_list.hpp"

namespace benzin
{

    template <typename T>
    class CommandQueue
    {
    public:
        BENZIN_NON_COPYABLE_IMPL(CommandQueue)
        BENZIN_NON_MOVEABLE_IMPL(CommandQueue)
        BENZIN_DX_DEBUG_NAME_IMPL(m_D3D12CommandQueue)

    public:
        CommandQueue(Device& device, size_t commandAllocatorCount);
        virtual ~CommandQueue();

    public:
        ID3D12CommandQueue* GetD3D12CommandQueue() const { BENZIN_ASSERT(m_D3D12CommandQueue); return m_D3D12CommandQueue; }

    public:
        void ResetCommandList(uint32_t commandAllocatorIndex);
        void ExecuteCommandList();
        void Flush();
        void FlushAndResetCommandList();
        void UpdateFenceValue(Fence& fence, uint64_t value);

    protected:
        virtual void InitCommandList() = 0;

    protected:
        Device& m_Device;

        ID3D12CommandQueue* m_D3D12CommandQueue{ nullptr };
        std::vector<ID3D12CommandAllocator*> m_D3D12CommandAllocators;

        T m_CommandList;

        Fence m_FlushFence;
        uint64_t m_FlushCount{ 0 };
    };

    template <typename T>
    class CommandQueueScope
    {
    public:
        explicit CommandQueueScope(T& commandQueue)
            : m_CommandQueue{ commandQueue }
        {}

        ~CommandQueueScope()
        {
            m_CommandQueue.FlushAndResetCommandList();
        }

    public:
        T* operator->() { return &m_CommandQueue; }
        T& operator*() { return m_CommandQueue; }

    private:
        T& m_CommandQueue;
    };

    class CopyCommandQueue : public CommandQueue<CopyCommandList>
    {
    public:
        explicit CopyCommandQueue(Device& device);
        ~CopyCommandQueue() override = default;

    public:
        CopyCommandList& GetCommandList(uint32_t uploadBufferSize = 0);

    private:
        void InitCommandList() override;
    };

    class ComputeCommandQueue : public CommandQueue<ComputeCommandList>
    {
    public:
        explicit ComputeCommandQueue(Device& device);
        ~ComputeCommandQueue() override = default;

    public:
        ComputeCommandList& GetCommandList() { ResetCommandList(0);  return m_CommandList; }

    private:
        void InitCommandList() override;
    };

    class GraphicsCommandQueue : public CommandQueue<GraphicsCommandList>
    {
    public:
        explicit GraphicsCommandQueue(Device& device);
        ~GraphicsCommandQueue() override = default;

    public:
        GraphicsCommandList& GetCommandList() { return m_CommandList; }

    private:
        void InitCommandList() override;
    };

} // namespace benzin

#include "benzin/graphics/api/command_queue.inl"
