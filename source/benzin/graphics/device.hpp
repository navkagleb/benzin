#pragma once

#include <benzin/graphics/descriptor_manager.hpp>

namespace benzin
{

    class Backend;
    class ConstBufferLinearAllocator;
    class Fence;
    class GpuHeap;
    class GpuHeapLinearBufferAllocator;
    class GraphicsCmdQueue;
    class QueryHeap;
    class Resource;
    class UnifiedRootSignature;

    struct DeviceCreation
    {
        std::string_view m_DebugName;
        Backend& m_Backend;
    };

    struct DeviceCaps
    {
        bool IsGpuUploadHeapsSupported = false;
    };

    class Device
    {
    public:
        friend class SwapChain;

        explicit Device(const DeviceCreation& creation);
        ~Device();

        BenzinDefineNonCopyable(Device);
        BenzinDefineNonMoveable(Device);

    public:
        auto* GetD3D12Device() const { return m_D3D12Device; }

        auto& GetUnifiedRootSignature() { return *m_UnifiedRootSignature; }
        auto& GetDescriptorManager() { return *m_DescriptorManager; }
        auto& GetGraphicsCmdQueue() { return *m_GraphicsCmdQueue; }

        auto GetCpuFrameIndex() const { return m_CpuFrameIndex; }
        auto GetCompletedGpuFrameIndex() const { return m_CompletedGpuFrameIndex; }
        auto GetActiveFrameIndex() const { return m_ActiveFrameIndex; }

        const auto& GetCaps() const { return m_Caps; }

        auto& GetTemporalLinearAllocator() { return *m_TemporalLinearAllocators[m_ActiveFrameIndex]; }
        auto& GetPersistentDefaultLinearAllocator() { return *m_PersistentDefaultLinearAllocator; }
        auto& GetPersistentReadbackLinearAllocator() { return *m_PersistentReadbackLinearAllocator; }
        const auto& GetPrevTemporalLinearBufferAllocator() const { return *m_TemporalLinearAllocators[(m_ActiveFrameIndex + 1) % BENZIN_FRAME_COUNT]; }

        auto& GetConstBufferAllocator() { return *m_ConstBufferAllocator; }

        uint8_t GetPlaneCountFromFormat(DXGI_FORMAT dxgiFormat) const;

        void DeferredRelease(ID3D12Object* d3d12Object);
        void DeferredRelease(const Descriptor& descriptor);
        void ProcessDeferredReleaseQueues(bool isForceRelease = false); // Must be called after 'SwapChain::OnFlip' because 'm_CompletedGpuFrameIndex' will be updated there

        void SignalFrameFence();
        void WaitForGpuIfNeeded();
        void AdvanceFrame(uint32_t activeFrameIndex);

    private:
        void CheckFeaturesSupport();

        // ID3D12Device5 supports RT
        ID3D12Device5* m_D3D12Device = nullptr;

        // Must be released in destructor before m_D3D12Device destroying
        // std::unique_ptr to indicate that the device owns these member lifetime
        std::unique_ptr<UnifiedRootSignature> m_UnifiedRootSignature;
        std::unique_ptr<DescriptorManager> m_DescriptorManager;
        std::unique_ptr<GraphicsCmdQueue> m_GraphicsCmdQueue;
        std::unique_ptr<Fence> m_FrameFence;

        std::unique_ptr<GpuHeap> m_TemporalHeaps[BENZIN_FRAME_COUNT];
        std::unique_ptr<GpuHeapLinearBufferAllocator> m_TemporalLinearAllocators[BENZIN_FRAME_COUNT];

        std::unique_ptr<GpuHeap> m_PersistentDefaultHeap;
        std::unique_ptr<GpuHeap> m_PersistentReadbackHeap;

        std::unique_ptr<GpuHeapLinearBufferAllocator> m_PersistentDefaultLinearAllocator;
        std::unique_ptr<GpuHeapLinearBufferAllocator> m_PersistentReadbackLinearAllocator;

        std::unique_ptr<ConstBufferLinearAllocator> m_ConstBufferAllocator;

        uint64_t m_CpuFrameIndex = 0;
        uint64_t m_CompletedGpuFrameIndex = 0;
        uint32_t m_ActiveFrameIndex = 0; // In range [0, FrameInFlightCount)

        DeviceCaps m_Caps;

        std::queue<std::pair<uint64_t, ID3D12Object*>> m_DeferredReleaseResourceQueue;
        std::queue<std::pair<uint64_t, Descriptor>> m_DeferredReleaseDescriptorQueue;
    };

}
