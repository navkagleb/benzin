#include "constant_buffer.h"

#include "resource.h"
#include "descriptor_heap.h"

namespace Spieler
{

    template <typename T>
    bool ConstantBuffer::InitAsRootDescriptorTable(UploadBuffer* uploadBuffer, std::uint32_t index, const DescriptorHeap& heap, std::uint32_t heapIndex)
    {
        SPIELER_RETURN_IF_FAILED(Init<T>(uploadBuffer, index));

        m_HeapIndex = heapIndex;

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation  = m_UploadBuffer->GetGPUVirtualAddress(m_Index);
        cbvDesc.SizeInBytes     = _Internal::CalcConstantBufferSize(sizeof(T));

        GetDevice()->CreateConstantBufferView(&cbvDesc, heap.GetCPUHandle(heapIndex));

        return true;
    }

    template <typename T>
    bool ConstantBuffer::InitAsRootDescriptor(UploadBuffer* uploadBuffer, std::uint32_t index)
    {
        return Init<T>(uploadBuffer, index);
    }

    template <typename T>
    bool ConstantBuffer::Init(UploadBuffer* uploadBuffer, std::uint32_t index)
    {
        m_UploadBuffer  = uploadBuffer;
        m_Index         = index;

        return true;
    }

} // namespace Spieler