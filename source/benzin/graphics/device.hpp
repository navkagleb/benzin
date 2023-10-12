#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Backend;

    class CopyCommandQueue;
    class ComputeCommandQueue;
    class GraphicsCommandQueue;

    class DescriptorManager;
    class TextureLoader;

    class Device
    {
    public:
        BENZIN_NON_COPYABLE_IMPL(Device)
        BENZIN_NON_MOVEABLE_IMPL(Device)

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

#if BENZIN_IS_DEBUG_BUILD
        void BreakOnD3D12Error(bool isBreak);
        void ReportLiveObjects();
#endif

    private:
        // ID3D12Device5 supports RT
        ID3D12Device5* m_D3D12Device = nullptr;

        ID3D12RootSignature* m_D3D12BindlessRootSignature = nullptr;

        CopyCommandQueue* m_CopyCommandQueue = nullptr;
        ComputeCommandQueue* m_ComputeCommandQueue = nullptr;
        GraphicsCommandQueue* m_GraphicsCommandQueue = nullptr;

        DescriptorManager* m_DescriptorManager = nullptr;
        TextureLoader* m_TextureLoader = nullptr;
    };

} // namespace benzin
