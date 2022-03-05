#pragma once

#include "bindable.h"

namespace Spieler
{

    enum DescriptorHeapType : std::uint32_t
    {
        DescriptorHeapType_CBVSRVUAV    = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        DescriptorHeapType_RTV          = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        DescriptorHeapType_DSV          = D3D12_DESCRIPTOR_HEAP_TYPE_DSV
    };

    class DescriptorHeap : public Bindable
    {
    public:
        ID3D12DescriptorHeap* GetRaw() { return m_Heap.Get(); }
        const ID3D12DescriptorHeap* GetRaw() const { return m_Heap.Get(); }

        std::uint32_t GetSize() const { return m_Size; }

    public:
        bool Init(DescriptorHeapType type, std::uint32_t descriptorCount)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            desc.Type           = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type);
            desc.NumDescriptors = descriptorCount;
            desc.NodeMask       = 0;

            if (type == DescriptorHeapType_CBVSRVUAV)
            {
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            }
            else
            {
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            }

            GetDevice()->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_Heap);

            m_Size = GetDevice()->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));

            return true;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(std::uint32_t index) const
        {
            return { m_Heap->GetCPUDescriptorHandleForHeapStart().ptr + index * m_Size };
        }

        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(std::uint32_t index) const
        {
            return { m_Heap->GetGPUDescriptorHandleForHeapStart().ptr + index * m_Size };
        }

        void Bind() override
        {
            GetCommandList()->SetDescriptorHeaps(1, m_Heap.GetAddressOf());
        }

    private:
        ComPtr<ID3D12DescriptorHeap>    m_Heap;
        std::uint32_t                   m_Size = 0;
    };

} // namespace Spieler