namespace Spieler
{

    template <typename Vertex>
    bool VertexBuffer::Init(const Vertex* vertices, const std::uint32_t count)
    {
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment          = 0;
        resourceDesc.Width              = sizeof(Vertex) * count;
        resourceDesc.Height             = 1;
        resourceDesc.DepthOrArraySize   = 1;
        resourceDesc.MipLevels          = 1;
        resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count   = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;
   
        m_Resource = std::move(Resource::CreateDefaultBuffer(resourceDesc));
        
        if (!m_Resource)
        {
            return false;
        }

        m_UploadBuffer = std::move(Resource::CreateUploadBuffer(resourceDesc));

        if (!m_UploadBuffer)
        {
            return false;
        }

        CopyDataToDefaultBuffer<Vertex>(m_Resource, m_UploadBuffer, vertices, count);
        
        m_View.BufferLocation    = m_Resource->GetGPUVirtualAddress();
        m_View.StrideInBytes     = sizeof(Vertex);
        m_View.SizeInBytes       = sizeof(Vertex) * count;

        return true;
    }

} // namespace Spieler