#pragma once

#include "bindable.hpp"

namespace Spieler
{

    using CPUDescriptorHandle = std::uint64_t;
    using GPUDescriptorHandle = std::uint64_t;

    enum class DescriptorHeapType : std::uint32_t
    {
        CBV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        SRV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        CBVSRVUAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        Sampler = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        RTV = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        DSV = D3D12_DESCRIPTOR_HEAP_TYPE_DSV
    };

    class DescriptorHeap : public Bindable
    {
    public:
        std::uint32_t GetSize() const { return m_Size; }
        std::uint32_t GetDescriptorCount() const { return m_DescriptorCount; }

    public:
        bool Init(DescriptorHeapType type, std::uint32_t descriptorCount);

        void Bind() const override;

        CPUDescriptorHandle GetDescriptorCPUHandle(std::uint32_t index) const;
        GPUDescriptorHandle GetDescriptorGPUHandle(std::uint32_t index) const;

    public:
        explicit operator ID3D12DescriptorHeap* () { return m_Heap.Get(); }

    private:
        ComPtr<ID3D12DescriptorHeap> m_Heap;
        std::uint32_t m_Size{ 0 };
        std::uint32_t m_DescriptorCount{ 0 };
    };

} // namespace Spieler