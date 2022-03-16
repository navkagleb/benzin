#include "renderer/constant_buffer.h"

#include "renderer/descriptor_heap.h"

namespace Spieler
{

    ConstantBuffer::ConstantBuffer(ConstantBuffer&& other) noexcept
        : m_UploadBuffer(std::exchange(other.m_UploadBuffer, nullptr))
        , m_MappedData(std::exchange(other.m_MappedData, nullptr))
        , m_Index(std::exchange(other.m_Index, 0))
    {}

    ConstantBuffer::~ConstantBuffer()
    {
        if (m_UploadBuffer)
        {
            m_UploadBuffer->Unmap(0, nullptr);
        }

        m_MappedData = nullptr;
    }

    void ConstantBuffer::BindAsRootDescriptorTable(const DescriptorHeap& heap) const
    {
        GetCommandList()->SetGraphicsRootDescriptorTable(0, heap.GetGPUHandle(m_Index));
    }

    void ConstantBuffer::BindAsRootDescriptor(std::uint32_t registerIndex) const
    {
        GetCommandList()->SetGraphicsRootConstantBufferView(registerIndex, m_UploadBuffer->GetGPUVirtualAddress());
    }

} // namespace Spieler