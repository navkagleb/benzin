#include "renderer/constant_buffer.h"

#include "renderer/descriptor_heap.h"

namespace Spieler
{

    bool ConstantBuffer::InitAsRootDescriptorTable(UploadBuffer* uploadBuffer, std::uint32_t index, const DescriptorHeap& heap, std::uint32_t heapIndex)
    {
        SPIELER_RETURN_IF_FAILED(Init(uploadBuffer, index));

        m_HeapIndex = heapIndex;

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation  = m_UploadBuffer->GetGPUVirtualAddress(m_Index);
        cbvDesc.SizeInBytes     = m_UploadBuffer->GetStride();

        GetDevice()->CreateConstantBufferView(&cbvDesc, heap.GetCPUHandle(heapIndex));

        return true;
    }

    bool ConstantBuffer::InitAsRootDescriptor(UploadBuffer* uploadBuffer, std::uint32_t index)
    {
        return Init(uploadBuffer, index);
    }

    bool ConstantBuffer::Init(UploadBuffer* uploadBuffer, std::uint32_t index)
    {
        m_UploadBuffer  = uploadBuffer;
        m_Index         = index;

        return true;
    }
   
    void ConstantBuffer::BindAsRootDescriptorTable(const DescriptorHeap& heap) const
    {
        GetCommandList()->SetGraphicsRootDescriptorTable(0, heap.GetGPUHandle(m_HeapIndex));
    }

    void ConstantBuffer::BindAsRootDescriptor(std::uint32_t registerIndex) const
    {
        GetCommandList()->SetGraphicsRootConstantBufferView(registerIndex, m_UploadBuffer->GetGPUVirtualAddress(m_Index));
    }

} // namespace Spieler