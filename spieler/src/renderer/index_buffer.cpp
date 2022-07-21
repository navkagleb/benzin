#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/index_buffer.hpp"

#include "spieler/core/assert.hpp"

#include "spieler/renderer/common.hpp"
#include "spieler/renderer/context.hpp"
#include "spieler/renderer/buffer.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler::renderer
{

    IndexBufferView::IndexBufferView(const BufferResource& resource)
    {
        Init(resource);
    }

    void IndexBufferView::Init(const BufferResource& resource)
    {
        SPIELER_ASSERT(resource.GetResource());

        const GraphicsFormat format{ resource.GetStride() == 2 ? GraphicsFormat::R16UnsignedInt : GraphicsFormat::R32UnsignedInt };

        m_View.BufferLocation = static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(resource.GetGPUVirtualAddress());
        m_View.SizeInBytes = resource.GetSize();
        m_View.Format = dx12::Convert(format);
    }

    void IndexBufferView::Bind(Context& context) const
    {
        context.GetNativeCommandList()->IASetIndexBuffer(&m_View);
    }

    void IndexBuffer::SetResource(const std::shared_ptr<BufferResource>& resource)
    {
        SPIELER_ASSERT(resource);

        m_Resource = resource;
        m_View = IndexBufferView{ *m_Resource };
    }

} // namespace spieler::renderer