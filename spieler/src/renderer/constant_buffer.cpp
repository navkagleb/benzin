#include "renderer/constant_buffer.h"

#include "renderer/descriptor_heap.h"

namespace Spieler
{

   
    void ConstantBuffer::BindAsRootDescriptorTable(const DescriptorHeap& heap) const
    {
        GetCommandList()->SetGraphicsRootDescriptorTable(0, heap.GetGPUHandle(m_HeapIndex));
    }

    void ConstantBuffer::BindAsRootDescriptor(std::uint32_t registerIndex) const
    {
        GetCommandList()->SetGraphicsRootConstantBufferView(registerIndex, m_UploadBuffer->GetGPUVirtualAddress(m_Index));
    }

} // namespace Spieler