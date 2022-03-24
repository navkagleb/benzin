#include "renderer/index_buffer.hpp"

namespace Spieler
{

    void IndexBuffer::Bind() const
    {
        GetCommandList()->IASetIndexBuffer(&m_View);
    }

} // namespace Spieler