#include "index_buffer.h"

namespace Spieler
{

    void IndexBuffer::Bind()
    {
        GetCommandList()->IASetIndexBuffer(&m_View);
    }

} // namespace Spieler