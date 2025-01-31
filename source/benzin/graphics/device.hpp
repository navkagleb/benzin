#pragma once

#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    class Backend;
    class GraphicsCommandQueue;
    class Pso;
    class QueryHeap;
    class RayTracing_Pso;
    class Resource;
    class UnifiedRootSignature;

    struct DeviceCreation
    {
        std::string_view DebugName;

        Backend& BackendRef;
    };

    struct DeviceCaps
    {
        bool IsGpuUploadHeapsSupported = false;
        bool IsDredSupported = false;
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
        auto& GetBackend() { return m_Backend; }
        const auto& GetBackend() const { return m_Backend; }

        auto* GetD3D12Device() const { return m_D3D12Device; }

        auto& GetUnifiedRootSignature() { return *m_UnifiedRootSignature; }
        auto& GetDescriptorManager() { return *m_DescriptorManager; }
        auto& GetGraphicsCommandQueue() { return *m_GraphicsCommandQueue; }

        auto GetCpuFrameIndex() const { return m_CpuFrameIndex; }
        auto GetCompletedGpuFrameIndex() const { return m_CompletedGpuFrameIndex; }
        auto GetActiveFrameIndex() const { return m_ActiveFrameIndex; }

        const auto& GetCaps() const { return m_Caps; }

        uint8_t GetPlaneCountFromFormat(GraphicsFormat format) const;

        void DeferredRelease(const Descriptor& descriptor);
        void DeferredRelease(const Pso& pso);
        void DeferredRelease(const QueryHeap& queryHeap);
        void DeferredRelease(const RayTracing_Pso& pso);
        void DeferredRelease(const Resource& resource);
        void ProcessDeferredReleaseQueues(bool isForceRelease = false); // Must be called after 'SwapChain::OnFlip' because 'm_CompletedGpuFrameIndex' will be updated there

    private:
        void CheckFeaturesSupport();

        void DeferredRelease(ID3D12Object* d3d12Object);

    private:
        Backend& m_Backend;

        // ID3D12Device5 supports RT
        ID3D12Device5* m_D3D12Device = nullptr;

        // Must be released in desctructor before m_D3D12Device destroying
        std::unique_ptr<UnifiedRootSignature> m_UnifiedRootSignature;
        std::unique_ptr<DescriptorManager> m_DescriptorManager;
        std::unique_ptr<GraphicsCommandQueue> m_GraphicsCommandQueue;

        uint64_t m_CpuFrameIndex = 0;
        uint64_t m_CompletedGpuFrameIndex = 0;
        uint8_t m_ActiveFrameIndex = 0; // In range [0, FrameInFlightCount)

        DeviceCaps m_Caps;

        std::queue<std::pair<uint64_t, ID3D12Object*>> m_DeferredReleaseResourceQueue;
        std::queue<std::pair<uint64_t, Descriptor>> m_DeferredReleaseDescriptorQueue;
    };

} // namespace benzin
