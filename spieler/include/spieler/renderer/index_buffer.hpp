#pragma once

#include "resource.hpp"

namespace Spieler
{

    template <typename T>
    concept IndexType = std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::uint32_t>;

    class UploadBuffer;

    class IndexBuffer : public RendererObject
    {
    public:
        friend class IndexBufferView;

    public:
        std::uint32_t GetIndexCount() const { return m_IndexCount; }

    public:
        template <IndexType T>
        bool Init(const T* indices, std::uint32_t count, UploadBuffer& uploadBuffer);

    private:
        ComPtr<ID3D12Resource> m_Buffer;
        std::uint32_t m_IndexCount{ 0 };
        std::uint32_t m_IndexTypeBitCount{ 0 };
    };

    class IndexBufferView : public RendererObject
    {
    public:
        IndexBufferView() = default;
        IndexBufferView(const IndexBuffer& indexBuffer);

    public:
        void Init(const IndexBuffer& indexBuffer);

        void Bind() const;

    private:
        D3D12_INDEX_BUFFER_VIEW m_View{};
    };

} // namespace Spieler

#include "index_buffer.inl"