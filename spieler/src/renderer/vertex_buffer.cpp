#include "renderer/vertex_buffer.hpp"

#include "renderer/upload_buffer.hpp"

namespace Spieler
{

    bool VertexBuffer::Init(const UploadBuffer& uploadBuffer)
    {
        m_View.BufferLocation = uploadBuffer.GetGPUVirtualAddress();
        m_View.StrideInBytes = uploadBuffer.GetStride();
        m_View.SizeInBytes = uploadBuffer.GetStride() * uploadBuffer.GetElementCount();

        return true;
    }

    void VertexBuffer::Bind() const
    {
        GetCommandList()->IASetVertexBuffers(0, 1, &m_View);
    }

} // namespace Spieler