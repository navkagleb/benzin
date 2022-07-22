#pragma once

namespace spieler::renderer
{

    class BufferResource;
    class Context;

    class VertexBufferView
    {
    public:
        VertexBufferView() = default;
        VertexBufferView(const BufferResource& resource);

    private:
        void Init(const BufferResource& resource);

    private:
        D3D12_VERTEX_BUFFER_VIEW m_View{};
    };

    class VertexBuffer
    {
    public:
        std::shared_ptr<BufferResource>& GetResource() { return m_Resource; }
        const std::shared_ptr<BufferResource>& GetResource() const { return m_Resource; }

        const VertexBufferView& GetView() const { return m_View; }

    public:
        void SetResource(const std::shared_ptr<BufferResource>& resource);

    private:
        std::shared_ptr<BufferResource> m_Resource;
        VertexBufferView m_View;
    };

} // namespace spieler::renderer