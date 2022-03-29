#include "renderer/constant_buffer.hpp"

#include "renderer/descriptor_heap.hpp"

namespace Spieler
{

    ConstantBuffer::ConstantBuffer(UploadBuffer& uploadBuffer, std::uint32_t index)
    {
        Init(uploadBuffer, index);
    }

    void ConstantBuffer::Init(UploadBuffer& uploadBuffer, std::uint32_t index)
    {
        m_UploadBuffer = &uploadBuffer;
        m_Index = index;
    }

    void ConstantBuffer::Bind(std::uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_UploadBuffer);

        GetCommandList()->SetGraphicsRootConstantBufferView(rootParameterIndex, m_UploadBuffer->GetElementGPUVirtualAddress(m_Index));
    }

    ConstantBufferView::ConstantBufferView(const ConstantBuffer& constantBuffer, const DescriptorHeap& descriptorHeap, std::uint32_t descriptorHeapIndex)
    {
        Init(constantBuffer, descriptorHeap, descriptorHeapIndex);
    }

    void ConstantBufferView::Init(const ConstantBuffer& constantBuffer, const DescriptorHeap& descriptorHeap, std::uint32_t descriptorHeapIndex)
    {
        m_DescriptorHeap = &descriptorHeap;
        m_DescriptorHeapIndex = descriptorHeapIndex;

        const UploadBuffer* const uploadBuffer{ constantBuffer.m_UploadBuffer };
        const std::uint32_t index{ constantBuffer.m_Index };

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = uploadBuffer->GetElementGPUVirtualAddress(index);
        cbvDesc.SizeInBytes = uploadBuffer->GetStride();

        GetDevice()->CreateConstantBufferView(&cbvDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ m_DescriptorHeap->GetDescriptorCPUHandle(m_DescriptorHeapIndex) });
    }

    void ConstantBufferView::Bind(std::uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_DescriptorHeap);

        GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE{ m_DescriptorHeap->GetDescriptorGPUHandle(m_DescriptorHeapIndex) });
    }

} // namespace Spieler