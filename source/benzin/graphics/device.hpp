#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Backend;
    class ComputeCommandQueue;
    class CopyCommandQueue;
    class DescriptorManager;
    class GraphicsCommandQueue;

    class Device
    {
    public:
        friend class SwapChain;

    public:
        BenzinDefineNonCopyable(Device);
        BenzinDefineNonMoveable(Device);

    public:
        explicit Device(const Backend& backend);
        ~Device();

    public:
        auto* GetD3D12Device() const { return m_D3D12Device; }
        auto* GetD3D12BindlessRootSignature() const { return m_D3D12BindlessRootSignature; }

        auto& GetDescriptorManager() { return *m_DescriptorManager; }

        auto& GetCopyCommandQueue() { return *m_CopyCommandQueue; }
        auto& GetComputeCommandQueue() { return *m_ComputeCommandQueue; }
        auto& GetGraphicsCommandQueue() { return *m_GraphicsCommandQueue; }

        auto GetCpuFrameIndex() const { return m_CpuFrameIndex; }
        auto GetGpuFrameIndex() const { return m_GpuFrameIndex; }
        auto GetActiveFrameIndex() const { return m_ActiveFrameIndex; }

        auto IsGpuUploadHeapsSupported() const { return m_IsGpuUploadHeapsSupported; }

    public:
        uint8_t GetPlaneCountFromFormat(GraphicsFormat format) const;

    private:
        void CheckFeaturesSupport();
        void CreateBindlessRootSignature();

    private:
        // ID3D12Device5 supports RT
        ID3D12Device5* m_D3D12Device = nullptr;

        ID3D12RootSignature* m_D3D12BindlessRootSignature = nullptr;

        std::unique_ptr<DescriptorManager> m_DescriptorManager;

        std::unique_ptr<CopyCommandQueue> m_CopyCommandQueue;
        std::unique_ptr<ComputeCommandQueue> m_ComputeCommandQueue;
        std::unique_ptr<GraphicsCommandQueue> m_GraphicsCommandQueue;

        uint64_t m_CpuFrameIndex = 0;
        uint64_t m_GpuFrameIndex = 0;
        uint8_t m_ActiveFrameIndex = 0; // In range [0, FrameInFlightCount)

        bool m_IsGpuUploadHeapsSupported = false;
    };

} // namespace benzin
