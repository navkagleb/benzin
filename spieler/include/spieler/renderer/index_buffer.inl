#include "upload_buffer.hpp"

namespace Spieler
{

    template <IndexType T>
    bool IndexBuffer::Init(const T* indices, std::uint32_t count, UploadBuffer& uploadBuffer)
    {
        SPIELER_ASSERT(indices != nullptr);
        SPIELER_ASSERT(count != 0);

        m_IndexCount = count;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = sizeof(T) * count;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        if constexpr (std::is_same_v<T, std::uint16_t>)
        {
            m_IndexTypeBitCount = 16;
        }
        else if constexpr (std::is_same_v<T, std::uint32_t>)
        {
            m_IndexTypeBitCount = 32;
        }
        else
        {
            return false;
        }

        SPIELER_RETURN_IF_FAILED(m_Buffer = std::move(Resource::CreateDefaultBuffer(resourceDesc)));
        SPIELER_RETURN_IF_FAILED(uploadBuffer.Init<T>(UploadBufferType::Default, count));

        Resource::CopyDataToDefaultBuffer<T>(m_Buffer.Get(), static_cast<ID3D12Resource*>(uploadBuffer), indices, count);

        return true;
    }

} // namespace Spieler