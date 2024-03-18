#pragma once

#include "benzin/graphics/command_list.hpp"

namespace benzin
{

    class Fence;

    template <std::derived_from<CommandList> CommandListT>
    class CommandQueue
    {
    public:
        BenzinDefineNonCopyable(CommandQueue);
        BenzinDefineNonMoveable(CommandQueue);

    public:
        CommandQueue(Device& device, size_t commandAllocatorCount);
        virtual ~CommandQueue();

    public:
        ID3D12CommandQueue* GetD3D12CommandQueue() const { return m_D3D12CommandQueue; }

        uint64_t GetTimestampFrequency() const;

    public:
        void ResetCommandList(uint32_t commandAllocatorIndex);
        void SumbitCommandList();
        void Flush();

        void SignalFence(Fence& fence, uint64_t value);

    protected:
        virtual void InitCommandList() = 0;

    protected:
        Device& m_Device;

        ID3D12CommandQueue* m_D3D12CommandQueue = nullptr;
        std::vector<ID3D12CommandAllocator*> m_D3D12CommandAllocators;

        CommandListT m_CommandList;

        std::unique_ptr<Fence> m_FlushFence;
        uint64_t m_FlushCount = 0;

        bool m_IsCommandListExecuted = false;
    };

    class CopyCommandQueue : public CommandQueue<CopyCommandList>
    {
    public:
        explicit CopyCommandQueue(Device& device);
        ~CopyCommandQueue() override = default;

    public:
        CopyCommandList& GetCommandList(uint32_t uploadBufferSize);

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

#define BenzinFlushCommandQueueOnScopeExit(commandQueue) BenzinExecuteOnScopeExit([&commandQueue] { commandQueue.SumbitCommandList(); commandQueue.Flush(); })

#include "benzin/graphics/command_queue.inl"
