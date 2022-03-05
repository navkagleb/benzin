#include "vertex_buffer.h"

namespace Spieler
{

    void VertexBuffer::Bind()
    {
        GetCommandList()->IASetVertexBuffers(0, 1, &m_View);
    }

} // namespace Spieler