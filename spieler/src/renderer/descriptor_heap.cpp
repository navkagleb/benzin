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

        if (type == DescriptorHeapType::CBVSRVUAV || type == DescriptorHeapType::Sampler)
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

    CPUDescriptorHandle DescriptorHeap::GetDescriptorCPUHandle(std::uint32_t index) const
    {
        SPIELER_ASSERT(m_Heap);

        return m_Heap->GetCPUDescriptorHandleForHeapStart().ptr + index * m_Size;
    }

    GPUDescriptorHandle DescriptorHeap::GetDescriptorGPUHandle(std::uint32_t index) const
    {
        SPIELER_ASSERT(m_Heap);

        return m_Heap->GetGPUDescriptorHandleForHeapStart().ptr + index * m_Size;
    }

} // namespace Spieler