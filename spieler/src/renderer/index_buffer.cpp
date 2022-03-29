#include "renderer/index_buffer.hpp"

namespace Spieler
{

    IndexBufferView::IndexBufferView(const IndexBuffer& indexBuffer)
    {
        Init(indexBuffer);
    }

    void IndexBufferView::Init(const IndexBuffer& indexBuffer)
    {
        SPIELER_ASSERT(indexBuffer.m_IndexTypeBitCount != 0);

        const D3D12_RESOURCE_DESC resourceDesc{ indexBuffer.m_Buffer->GetDesc() };

        m_View.BufferLocation = indexBuffer.m_Buffer->GetGPUVirtualAddress();
        m_View.SizeInBytes = static_cast<UINT>(resourceDesc.Width);
        m_View.Format = (indexBuffer.m_IndexTypeBitCount == 16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    }

    void IndexBufferView::Bind() const
    {
        GetCommandList()->IASetIndexBuffer(&m_View);
    }

} // namespace Spieler