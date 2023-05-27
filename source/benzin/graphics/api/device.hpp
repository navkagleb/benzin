#pragma once

#include "benzin/graphics/api/common.hpp"

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
        BENZIN_NON_COPYABLE(Device)
        BENZIN_NON_MOVEABLE(Device)
        BENZIN_DX_DEBUG_NAME_IMPL(m_D3D12Device)

    public:
        explicit Device(const Backend& backend);
        ~Device();

    public:
        ID3D12Device4* GetD3D12Device() const { BENZIN_ASSERT(m_D3D12Device); return m_D3D12Device; }
        ID3D12RootSignature* GetD3D12BindlessRootSignature() const { BENZIN_ASSERT(m_D3D12BindlessRootSignature); return m_D3D12BindlessRootSignature; }

        CopyCommandQueue& GetCopyCommandQueue() { return *m_CopyCommandQueue; }
        ComputeCommandQueue& GetComputeCommandQueue() { return *m_ComputeCommandQueue; }
        GraphicsCommandQueue& GetGraphicsCommandQueue() { return *m_GraphicsCommandQueue; }

        DescriptorManager& GetDescriptorManager() { BENZIN_ASSERT(m_DescriptorManager); return *m_DescriptorManager; }
        TextureLoader& GetTextureLoader() { BENZIN_ASSERT(m_TextureLoader);  return *m_TextureLoader; }

    private:
        void CreateBindlessRootSignature();

#if defined(BENZIN_DEBUG_BUILD)
        void BreakOnD3D12Error(bool isBreak);
        void ReportLiveObjects();
#endif

    private:
        ID3D12Device4* m_D3D12Device{ nullptr };

        ID3D12RootSignature* m_D3D12BindlessRootSignature{ nullptr };

        CopyCommandQueue* m_CopyCommandQueue{ nullptr };
        ComputeCommandQueue* m_ComputeCommandQueue{ nullptr };
        GraphicsCommandQueue* m_GraphicsCommandQueue{ nullptr };

        DescriptorManager* m_DescriptorManager{ nullptr };
        TextureLoader* m_TextureLoader{ nullptr };
    };

} // namespace benzin
