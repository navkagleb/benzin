#include "renderer/vertex_buffer.h"

#include "renderer/upload_buffer.h"

namespace Spieler
{

    bool VertexBuffer::Init(const UploadBuffer& uploadBuffer)
    {
        m_View.BufferLocation   = uploadBuffer.GetGPUVirtualAddress();
        m_View.StrideInBytes    = uploadBuffer.GetStride();
        m_View.SizeInBytes      = uploadBuffer.GetStride() * uploadBuffer.GetElementCount();

        return true;
    }

    void VertexBuffer::Bind() const
    {
        GetCommandList()->IASetVertexBuffers(0, 1, &m_View);
    }

} // namespace Spieler