#pragma once

#include <benzin/graphics/cmd_list.hpp>

namespace benzin
{

    class Fence;

    class GraphicsCmdQueue
    {
    public:
        GraphicsCmdQueue(Device& device);
        ~GraphicsCmdQueue();

        BenzinDefineNonCopyable(GraphicsCmdQueue);
        BenzinDefineNonMoveable(GraphicsCmdQueue);

    public:
        auto* GetD3D12CommandQueue() const { return m_D3D12CommandQueue; }

        GraphicsCmdList& GetCmdList(uint64_t uploadBufferSizeInBytes = 0);
        uint64_t GetTimestampFrequency() const;

        void ResetCmdList();
        void SubmitCmdList();

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
        GraphicsCmdList m_CmdList;

        std::unique_ptr<Fence> m_FlushFence;
        uint64_t m_FlushCount = 0;
    };

}
