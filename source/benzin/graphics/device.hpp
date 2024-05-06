#pragma once

#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    class Backend;
    class GpuTimer;
    class GraphicsCommandQueue;

    enum class UnifiedRootParameter
    {
        RootConstantBuffer,
        FrameConstantBuffer,
        RenderPassConstantBuffer,
        TopLevelAs,
    };
    BenzinEnableUnaryPlusForEnum(UnifiedRootParameter);

    struct DeviceCreation
    {
        std::string_view DebugName;
        const Backend& BackendRef;
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
        auto* GetD3D12UnifiedRootSignature() const { return m_D3D12UnifiedRootSignature; }

        auto& GetDescriptorManager() { return *m_DescriptorManager; }
        auto& GetGraphicsCommandQueue() { return *m_GraphicsCommandQueue; }

        auto& GetGpuTimer() { return *m_GpuTimer; }
        const auto& GetGpuTimer() const { return *m_GpuTimer; }

        auto GetCpuFrameIndex() const { return m_CpuFrameIndex; }
        auto GetCompletedGpuFrameIndex() const { return m_CompletedGpuFrameIndex; }
        auto GetActiveFrameIndex() const { return m_ActiveFrameIndex; }

        auto IsGpuUploadHeapsSupported() const { return m_IsGpuUploadHeapsSupported; }

        uint8_t GetPlaneCountFromFormat(GraphicsFormat format) const;

        void DeferredRelease(const Descriptor& descriptor);
        void DeferredRelease(ID3D12Object* d3d12Object);
        void ProcessDeferredReleaseQueues(bool isReleaseForced = false); // Must be called after 'SwapChain::OnFlip' because 'm_GpuFrameIndex' will be updated there

    private:
        void CheckFeaturesSupport();
        void CreateUnifiedRootSignature();

    private:
        // ID3D12Device5 supports RT
        ID3D12Device5* m_D3D12Device = nullptr;

        ID3D12RootSignature* m_D3D12UnifiedRootSignature = nullptr;

        std::unique_ptr<DescriptorManager> m_DescriptorManager;
        std::unique_ptr<GraphicsCommandQueue> m_GraphicsCommandQueue;
        std::unique_ptr<GpuTimer> m_GpuTimer;

        uint64_t m_CpuFrameIndex = 0;
        uint64_t m_CompletedGpuFrameIndex = 0;
        uint8_t m_ActiveFrameIndex = 0; // In range [0, FrameInFlightCount)

        bool m_IsGpuUploadHeapsSupported = false;

        std::queue<std::pair<uint64_t, ID3D12Object*>> m_DeferredReleaseResourceQueue;
        std::queue<std::pair<uint64_t, Descriptor>> m_DeferredReleaseDescriptorQueue;
    };

} // namespace benzin
