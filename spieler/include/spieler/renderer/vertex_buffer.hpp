#pragma once

#include "renderer_object.hpp"

namespace Spieler
{

    class UploadBuffer;

    class VertexBuffer : public RendererObject
    {
    public:
        friend class VertexBufferView;

    public:
        std::uint32_t GetVertexCount() const { return m_VertexCount; }

    public:
        template <typename Vertex>
        bool Init(const Vertex* vertices, std::uint32_t count, UploadBuffer& uploadBuffer);

    private:
        ComPtr<ID3D12Resource> m_Buffer;
        std::uint32_t m_VertexCount{ 0 };
    };

    class VertexBufferView : public RendererObject
    {
    public:
        VertexBufferView() = default;
        VertexBufferView(const VertexBuffer& vertexBuffer);
        VertexBufferView(const UploadBuffer& uploadBuffer);

    public:
        void Init(const VertexBuffer& vertexBuffer);
        void Init(const UploadBuffer& uploadBuffer);

        void Bind() const;

    private:
        D3D12_VERTEX_BUFFER_VIEW m_View{};
    };

} // namespace Spieler

#include "vertex_buffer.inl"