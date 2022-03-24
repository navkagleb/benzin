#pragma once

#include "bindable.hpp"

namespace Spieler
{

    enum DescriptorHeapType : std::uint32_t
    {
        DescriptorHeapType_CBV          = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        DescriptorHeapType_SRV          = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        DescriptorHeapType_UAV          = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        DescriptorHeapType_CBVSRVUAV    = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        DescriptorHeapType_RTV          = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        DescriptorHeapType_DSV          = D3D12_DESCRIPTOR_HEAP_TYPE_DSV
    };

    class DescriptorHeap : public Bindable
    {
    public:
        std::uint32_t GetSize() const { return m_Size; }
        std::uint32_t GetDescriptorCount() const { return m_DescriptorCount; }

    public:
        bool Init(DescriptorHeapType type, std::uint32_t descriptorCount);

        void Bind() const override;

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(std::uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(std::uint32_t index) const;

    public:
        explicit operator ID3D12DescriptorHeap* () { return m_Heap.Get(); }

    private:
        ComPtr<ID3D12DescriptorHeap>    m_Heap;
        std::uint32_t                   m_Size              = 0;
        std::uint32_t                   m_DescriptorCount   = 0;
    };

} // namespace Spieler