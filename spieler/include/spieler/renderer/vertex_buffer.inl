#include "resource.hpp"

#include "upload_buffer.hpp"

namespace Spieler
{

    template <typename Vertex>
    bool VertexBuffer::Init(const Vertex* vertices, const std::uint32_t count, UploadBuffer& uploadBuffer)
    {
        SPIELER_ASSERT(vertices);
        SPIELER_ASSERT(count != 0);

        m_VertexCount = count;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = sizeof(Vertex) * count;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
   
        m_Buffer = std::move(Resource::CreateDefaultBuffer(resourceDesc));
        
        SPIELER_RETURN_IF_FAILED(m_Buffer);
        SPIELER_RETURN_IF_FAILED(uploadBuffer.Init<Vertex>(UploadBufferType::Default, count));

        Resource::CopyDataToDefaultBuffer<Vertex>(m_Buffer.Get(), static_cast<ID3D12Resource*>(uploadBuffer), vertices, count);

        return true;
    }

} // namespace Spieler