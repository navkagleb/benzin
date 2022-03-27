#include "renderer/descriptor_heap.hpp"

namespace Spieler
{

    bool DescriptorHeap::Init(DescriptorHeapType type, std::uint32_t descriptorCount)
    {
        m_DescriptorCount = descriptorCount;

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type);
        desc.NumDescriptors = descriptorCount;
        desc.NodeMask = 0;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (type == DescriptorHeapType_CBVSRVUAV || type == DescriptorHeapType_Sampler)
        {
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        }

        SPIELER_RETURN_IF_FAILED(GetDevice()->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap), &m_Heap));

        m_Size = GetDevice()->GetDescriptorHandleIncrementSize(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(type));

        return true;
    }

    void DescriptorHeap::Bind() const
    {
        SPIELER_ASSERT(m_Heap);

        GetCommandList()->SetDescriptorHeaps(1, m_Heap.GetAddressOf());
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandle(std::uint32_t index) const
    {
        SPIELER_ASSERT(m_Heap);

        return { m_Heap->GetCPUDescriptorHandleForHeapStart().ptr + index * m_Size };
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHandle(std::uint32_t index) const
    {
        SPIELER_ASSERT(m_Heap);

        return { m_Heap->GetGPUDescriptorHandleForHeapStart().ptr + index * m_Size };
    }

} // namespace Spieler