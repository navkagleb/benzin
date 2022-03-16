#pragma once

#include "resource.h"

namespace Spieler
{

    template <typename T>
    concept IndexType = std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::uint32_t>;

    class IndexBuffer : public Resource
    {
    public:
        std::uint32_t GetIndexCount() const { return m_IndexCount; }

    public:
        template <IndexType T>
        bool Init(const T* indices, const std::uint32_t count);

        void Bind() const override;

    private:
        ComPtr<ID3D12Resource>      m_UploadBuffer;
        D3D12_INDEX_BUFFER_VIEW     m_View{};

        std::uint32_t               m_IndexCount{ 0 };
    };

} // namespace Spieler

#include "index_buffer.inl"