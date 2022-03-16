#include "renderer/index_buffer.h"

namespace Spieler
{

    void IndexBuffer::Bind() const
    {
        GetCommandList()->IASetIndexBuffer(&m_View);
    }

} // namespace Spieler