#pragma once

#include "benzin/graphics/fence.hpp"
#include "benzin/graphics/command_list.hpp"

namespace benzin
{

    template <typename T>
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

        uint64_t GetTimestampFrequency() const
        {
            BenzinAssert(m_D3D12CommandQueue);

            uint64_t frequency = 0;
            BenzinAssert(m_D3D12CommandQueue->GetTimestampFrequency(&frequency));

            return frequency;
        }

    public:

        void ResetCommandList(uint32_t commandAllocatorIndex);
        void ExecuteCommandList();
        void Flush();
        void UpdateFenceValue(Fence& fence, uint64_t value);

    protected:
        virtual void InitCommandList() = 0;

    protected:
        Device& m_Device;

        ID3D12CommandQueue* m_D3D12CommandQueue = nullptr;
        std::vector<ID3D12CommandAllocator*> m_D3D12CommandAllocators;

        T m_CommandList;

        Fence m_FlushFence;
        uint64_t m_FlushCount = 0;

        bool m_IsCommandListExecuted = false;
        bool m_IsNeedFlush = false;
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

#define BenzinFlushCommandQueueOnScopeExit(commandQueue) BenzinExecuteOnScopeExit([&commandQueue] { commandQueue.ExecuteCommandList(); commandQueue.Flush(); })

#include "benzin/graphics/command_queue.inl"
