#pragma once

#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    class Backend;
    class ComputePso;
    class ConstBufferLinearAllocator;
    class GpuHeap;
    class GpuHeapLinearBufferAllocator;
    class GraphicsCmdQueue;
    class MeshPso;
    class QueryHeap;
    class RayTracing_Pso;
    class Resource;
    class UnifiedRootSignature;
    class VertexPso;

    struct DeviceCreation
    {
        std::string_view DebugName;

        Backend& Backend;
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

        auto& GetConstBufferAllocator() { return *m_ConstBufferAllocator; }

        const GpuHeapLinearBufferAllocator& GetPrevTemporalLinearBufferAllocator() const;

        uint8_t GetPlaneCountFromFormat(GraphicsFormat format) const;

        void DeferredRelease(ID3D12Heap*& d3d12Heap);
        void DeferredRelease(ID3D12PipelineState*& d3d12PipelineState);
        void DeferredRelease(ID3D12QueryHeap*& d3d12QueryHeap);
        void DeferredRelease(ID3D12Resource*& d3d12Resource);
        void DeferredRelease(ID3D12StateObject*& d3d12StateObject);
        void DeferredRelease(const Descriptor& descriptor);
        void ProcessDeferredReleaseQueues(bool isForceRelease = false); // Must be called after 'SwapChain::OnFlip' because 'm_CompletedGpuFrameIndex' will be updated there

    private:
        void CheckFeaturesSupport();

        void DeferredRelease(ID3D12Object* d3d12Object);

    private:
        // ID3D12Device5 supports RT
        ID3D12Device5* m_D3D12Device = nullptr;

        // Must be released in destructor before m_D3D12Device destroying
        // std::unique_ptr to indicate that the device owns these member lifetime
        std::unique_ptr<UnifiedRootSignature> m_UnifiedRootSignature;
        std::unique_ptr<DescriptorManager> m_DescriptorManager;
        std::unique_ptr<GraphicsCmdQueue> m_GraphicsCmdQueue;

        std::vector<std::unique_ptr<GpuHeap>> m_TemporalHeaps;
        std::vector<std::unique_ptr<GpuHeapLinearBufferAllocator>> m_TemporalLinearAllocators;

        std::unique_ptr<GpuHeap> m_PersistentDefaultHeap;
        std::unique_ptr<GpuHeap> m_PersistentReadbackHeap;

        std::unique_ptr<GpuHeapLinearBufferAllocator> m_PersistentDefaultLinearAllocator;
        std::unique_ptr<GpuHeapLinearBufferAllocator> m_PersistentReadbackLinearAllocator;

        std::unique_ptr<ConstBufferLinearAllocator> m_ConstBufferAllocator;

        uint64_t m_CpuFrameIndex = 0;
        uint64_t m_CompletedGpuFrameIndex = 0;
        uint8_t m_ActiveFrameIndex = 0; // In range [0, FrameInFlightCount)

        DeviceCaps m_Caps;

        std::queue<std::pair<uint64_t, ID3D12Object*>> m_DeferredReleaseResourceQueue;
        std::queue<std::pair<uint64_t, Descriptor>> m_DeferredReleaseDescriptorQueue;
    };

} // namespace benzin
