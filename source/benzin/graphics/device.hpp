#pragma once

#include <benzin/graphics/descriptor_manager.hpp>

namespace benzin
{

    class Backend;
    class ConstBufferLinearAllocator;
    class Fence;
    class GpuHeap;
    class GpuHeapLinearAllocator;
    class GraphicsCmdQueue;
    class UnifiedRootSignature;

    struct DeviceCaps
    {
        bool m_IsGpuUploadHeapsSupported = false;
    };

    class Device
    {
    public:
        friend class SwapChain;

        explicit Device(std::string_view debugName, Backend& backend);
        ~Device();

        BenzinDefineNonCopyable(Device);
        BenzinDefineNonMoveable(Device);

        auto* GetD3D12Device() const { return m_D3D12Device; }

        auto& GetUnifiedRootSignature() { return *m_UnifiedRootSignature; }
        auto& GetDescriptorManager() { return *m_DescriptorManager; }
        auto& GetGraphicsCmdQueue() { return *m_GraphicsCmdQueue; }

        auto GetCpuFrameIndex() const { return m_CpuFrameIndex; }
        auto GetCompletedGpuFrameIndex() const { return m_CompletedGpuFrameIndex; }
        auto GetActiveFrameIndex() const { return m_ActiveFrameIndex; }
        auto GetGpuWaitTime() const { return m_GpuWaitTime; }

        const auto& GetCaps() const { return m_Caps; }

        decltype(auto) GetPersistentDefaultAllocator(this auto&& self) { return *self.m_PersistentDefaultAllocator; }
        decltype(auto) GetPersistentGpuUploadAllocator(this auto&& self) { return *self.m_PersistentGpuUploadAllocator; }
        decltype(auto) GetPersistentReadbackAllocator(this auto&& self) { return *self.m_PersistentReadbackAllocator; }
        decltype(auto) GetResDependentAllocator(this auto&& self) { return *self.m_ResDependentAllocator; }

        auto& GetConstBufferAllocator() { return *m_ConstBufferAllocator; }

        uint32_t GetReadbackWriteIndex() const { return m_CpuFrameIndex % BENZIN_READBACK_LATENCY; }
        uint32_t GetReadbackReadIndex() const { return (m_CpuFrameIndex + 1) % BENZIN_READBACK_LATENCY; }

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

        std::unique_ptr<GpuHeap> m_PersistentDefaultHeap;
        std::unique_ptr<GpuHeap> m_PersistentGpuUploadHeap;
        std::unique_ptr<GpuHeap> m_PersistentReadbackHeap;
        std::unique_ptr<GpuHeap> m_ResDependentHeap;

        std::unique_ptr<GpuHeapLinearAllocator> m_PersistentDefaultAllocator;
        std::unique_ptr<GpuHeapLinearAllocator> m_PersistentGpuUploadAllocator;
        std::unique_ptr<GpuHeapLinearAllocator> m_PersistentReadbackAllocator;
        std::unique_ptr<GpuHeapLinearAllocator> m_ResDependentAllocator;

        std::unique_ptr<ConstBufferLinearAllocator> m_ConstBufferAllocator;

        uint64_t m_CpuFrameIndex = 0;
        uint64_t m_CompletedGpuFrameIndex = 0;
        uint32_t m_ActiveFrameIndex = 0; // In range [0, FrameInFlightCount)
        std::chrono::nanoseconds m_GpuWaitTime = {};

        DeviceCaps m_Caps;

        std::queue<std::pair<uint64_t, ID3D12Object*>> m_DeferredReleaseResourceQueue;
        std::queue<std::pair<uint64_t, Descriptor>> m_DeferredReleaseDescriptorQueue;
    };

}
