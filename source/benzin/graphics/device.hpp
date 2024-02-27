#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Backend;
    class ComputeCommandQueue;
    class CopyCommandQueue;
    class DescriptorManager;
    class DeviceRemovedFence;
    class GraphicsCommandQueue;
    class TextureLoader;

    namespace rt
    {

        class AccelerationStructure;
        struct AccelerationStructureSizes;

    } // namespace rt

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
        ID3D12Device5* GetD3D12Device() const { return m_D3D12Device; }
        ID3D12RootSignature* GetD3D12BindlessRootSignature() const { return m_D3D12BindlessRootSignature; }

        CopyCommandQueue& GetCopyCommandQueue() { return *m_CopyCommandQueue; }
        ComputeCommandQueue& GetComputeCommandQueue() { return *m_ComputeCommandQueue; }
        GraphicsCommandQueue& GetGraphicsCommandQueue() { return *m_GraphicsCommandQueue; }

        DescriptorManager& GetDescriptorManager() { return *m_DescriptorManager; }
        TextureLoader& GetTextureLoader() { return *m_TextureLoader; }

        uint64_t GetCpuFrameIndex() const { return m_CpuFrameIndex; }
        uint64_t GetGpuFrameIndex() const { return m_GpuFrameIndex; }
        uint8_t GetActiveFrameIndex() const { return m_ActiveFrameIndex; }

    public:
        uint8_t GetPlaneCountFromFormat(GraphicsFormat format) const;

        rt::AccelerationStructureSizes GetAccelerationStructureSizes(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& d3d12BuildInputs);

    private:
        void CheckFeaturesSupport();
        void CreateBindlessRootSignature();

    private:
        // ID3D12Device5 supports RT
        ID3D12Device5* m_D3D12Device = nullptr;

        ID3D12RootSignature* m_D3D12BindlessRootSignature = nullptr;

        std::unique_ptr<CopyCommandQueue> m_CopyCommandQueue;
        std::unique_ptr<ComputeCommandQueue> m_ComputeCommandQueue;
        std::unique_ptr<GraphicsCommandQueue> m_GraphicsCommandQueue;

        std::unique_ptr<DescriptorManager> m_DescriptorManager;
        std::unique_ptr<TextureLoader> m_TextureLoader;

        uint64_t m_CpuFrameIndex = 0;
        uint64_t m_GpuFrameIndex = 0;
        uint8_t m_ActiveFrameIndex = 0; // In range [0, FrameInFlightCount)
    };

} // namespace benzin
