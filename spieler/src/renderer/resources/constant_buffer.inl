namespace Spieler
{

    namespace _Internal
    {

        inline static std::uint32_t CalcConstantBufferSize(std::uint32_t size)
        {
            return (size + 255) & ~255;
        }

    } // namespace _Internal

    template <typename T>
    ConstantBuffer<T>::~ConstantBuffer()
    {
        
        if (m_UploadBuffer)
        {
            m_UploadBuffer->Unmap(0, nullptr);
        }

        m_MappedData = nullptr;
    }

    template <typename T>
    bool ConstantBuffer<T>::Init(const D3D12_CPU_DESCRIPTOR_HANDLE& handle)
    {
        const std::uint32_t size = _Internal::CalcConstantBufferSize(sizeof(T));

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment          = 0;
        resourceDesc.Width              = size;
        resourceDesc.Height             = 1;
        resourceDesc.DepthOrArraySize   = 1;
        resourceDesc.MipLevels          = 1;
        resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count   = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        m_UploadBuffer = Resource::CreateUploadBuffer(resourceDesc);

        if (!m_UploadBuffer)
        {
            return false;
        }

        SPIELER_RETURN_IF_FAILED(m_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation  = m_UploadBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes     = size;
            
        GetDevice()->CreateConstantBufferView(&cbvDesc, handle);

        return true;
    }

} // namespace Spieler