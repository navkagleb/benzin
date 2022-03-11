namespace Spieler
{

    template <IndexType T>
    bool IndexBuffer::Init(const T* indices, std::uint32_t count)
    {
        m_IndexCount = count;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment          = 0;
        resourceDesc.Width              = sizeof(T) * count;
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

        CopyDataToDefaultBuffer<T>(m_Resource, m_UploadBuffer, indices, count);

        m_View.BufferLocation    = m_Resource->GetGPUVirtualAddress();
        m_View.SizeInBytes       = sizeof(T) * count;

        if constexpr (std::is_same_v<std::uint16_t, T>)
        {
            m_View.Format = DXGI_FORMAT_R16_UINT;
        }
        else if constexpr (std::is_same_v<std::uint32_t, T>)
        {
            m_View.Format = DXGI_FORMAT_R32_UINT;
        }
        else
        {
            return false;
        }

        return true;
    }

} // namespace Spieler