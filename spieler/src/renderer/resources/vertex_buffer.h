#pragma once

#include "resource.h"

namespace Spieler
{

    class VertexBuffer : public Resource
    {
    public:
        template <typename Vertex>
        bool Init(const Vertex* vertices, std::uint32_t count);

        void Bind() override;

    private:
        ComPtr<ID3D12Resource>      m_UploadBuffer;
        D3D12_VERTEX_BUFFER_VIEW    m_View{};
    };

} // namespace Spieler

#include "vertex_buffer.inl"