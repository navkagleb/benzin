#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Backend;
    class ComputeCommandQueue;
    class CopyCommandQueue;
    class DescriptorManager;
    class Fence;
    class GraphicsCommandQueue;
    class TextureLoader;

    class Device
    {
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

    public:
        uint8_t GetPlaneCountFromFormat(GraphicsFormat format) const;

    private:
        void CheckFeaturesSupport();
        void CreateBindlessRootSignature();

    private:
        // ID3D12Device5 supports RT
        ID3D12Device5* m_D3D12Device = nullptr;

        ID3D12RootSignature* m_D3D12BindlessRootSignature = nullptr;

        CopyCommandQueue* m_CopyCommandQueue = nullptr;
        ComputeCommandQueue* m_ComputeCommandQueue = nullptr;
        GraphicsCommandQueue* m_GraphicsCommandQueue = nullptr;

        DescriptorManager* m_DescriptorManager = nullptr;
        TextureLoader* m_TextureLoader = nullptr;

        Fence* m_DeviceRemovedFence = nullptr;
    };

} // namespace benzin
