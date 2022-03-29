#include "renderer/vertex_buffer.hpp"

#include "renderer/upload_buffer.hpp"

namespace Spieler
{

    VertexBufferView::VertexBufferView(const VertexBuffer& vertexBuffer)
    {
        Init(vertexBuffer);
    }

    VertexBufferView::VertexBufferView(const UploadBuffer& uploadBuffer)
    {
        Init(uploadBuffer);
    }

    void VertexBufferView::Init(const VertexBuffer& vertexBuffer)
    {
        const D3D12_RESOURCE_DESC resourceDesc{ vertexBuffer.m_Buffer->GetDesc() };

        m_View.BufferLocation = vertexBuffer.m_Buffer->GetGPUVirtualAddress();
        m_View.StrideInBytes = resourceDesc.Width / vertexBuffer.m_VertexCount;
        m_View.SizeInBytes = resourceDesc.Width;
    }

    void VertexBufferView::Init(const UploadBuffer& uploadBuffer)
    {
        m_View.BufferLocation = uploadBuffer.GetGPUVirtualAddress();
        m_View.StrideInBytes = uploadBuffer.GetStride();
        m_View.SizeInBytes = uploadBuffer.GetStride() * uploadBuffer.GetElementCount();
    }

    void VertexBufferView::Bind() const
    {
        GetCommandList()->IASetVertexBuffers(0, 1, &m_View);
    }

} // namespace Spieler