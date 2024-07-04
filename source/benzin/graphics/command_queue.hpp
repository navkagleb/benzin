#pragma once

#include "benzin/graphics/command_list.hpp"

namespace benzin
{

    class Fence;

    class GraphicsCommandQueue
    {
    public:
        GraphicsCommandQueue(Device& device);
        ~GraphicsCommandQueue();

        BenzinDefineNonCopyable(GraphicsCommandQueue);
        BenzinDefineNonMoveable(GraphicsCommandQueue);

    public:
        auto* GetD3D12CommandQueue() const { return m_D3D12CommandQueue; }

        GraphicsCommandList& GetCommandList(Bytes32 uploadBufferSize = 0);
        uint64_t GetTimestampFrequency() const;

        void OnFrameBegin();
        void OnFrameEnd();

        void SubmitCommandList();
        void Flush();

        void SignalFence(Fence& fence, uint64_t value);

    private:
        struct FrameContext
        {
            ID3D12CommandAllocator* D3D12CommandAllocator;
            std::vector<std::unique_ptr<Buffer>> UploadBuffers;
        };

        Device& m_Device;

        ID3D12CommandQueue* m_D3D12CommandQueue = nullptr;

        std::vector<FrameContext> m_FrameContexts;
        GraphicsCommandList m_CommandList;

        std::unique_ptr<Fence> m_FlushFence;
        uint64_t m_FlushCount = 0;
    };

} // namespace benzin
