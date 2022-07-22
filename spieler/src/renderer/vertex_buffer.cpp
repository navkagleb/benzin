#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/vertex_buffer.hpp"

#include "spieler/core/assert.hpp"

#include "spieler/renderer/buffer.hpp"
#include "spieler/renderer/context.hpp"

namespace spieler::renderer
{

    VertexBufferView::VertexBufferView(const BufferResource& resource)
    {
        Init(resource);
    }

    void VertexBufferView::Init(const BufferResource& resource)
    {
        SPIELER_ASSERT(resource.GetResource());

        m_View.BufferLocation = static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(resource.GetGPUVirtualAddress());
        m_View.StrideInBytes = resource.GetStride();
        m_View.SizeInBytes = resource.GetSize();
    }

    void VertexBuffer::SetResource(const std::shared_ptr<BufferResource>& resource)
    {
        SPIELER_ASSERT(resource);

        m_Resource = resource;
        m_View = VertexBufferView{ *m_Resource };
    }

} // namespace spieler::renderer