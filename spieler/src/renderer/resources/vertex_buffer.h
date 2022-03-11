#pragma once

#include "resource.h"

namespace Spieler
{

    class VertexBuffer : public Resource
    {
    public:
        std::uint32_t GetVertexCount() const { return m_VertexCount; }

    public:
        template <typename Vertex>
        bool Init(const Vertex* vertices, std::uint32_t count);

        void Bind() const override;

    private:
        ComPtr<ID3D12Resource>      m_UploadBuffer;
        D3D12_VERTEX_BUFFER_VIEW    m_View{};

        std::uint32_t               m_VertexCount = 0;
    };

} // namespace Spieler

#include "vertex_buffer.inl"