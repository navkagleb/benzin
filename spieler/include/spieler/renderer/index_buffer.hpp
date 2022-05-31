#pragma once

namespace spieler::renderer
{

    class BufferResource;
    class Context;

    class IndexBufferView
    {
    public:
        IndexBufferView() = default;
        IndexBufferView(const BufferResource& resource);

    public:
        void Bind(Context& context) const;

    public:
        void Init(const BufferResource& resource);

    private:
        D3D12_INDEX_BUFFER_VIEW m_View{};
    };

    class IndexBuffer
    {
    public:
        std::shared_ptr<BufferResource>& GetResource() { return m_Resource; }
        const std::shared_ptr<BufferResource>& GetResource() const { return m_Resource; }

        const IndexBufferView& GetView() const { return m_View; }

    public:
        void SetResource(const std::shared_ptr<BufferResource>& resource);

    private:
        std::shared_ptr<BufferResource> m_Resource;
        IndexBufferView m_View;
    };

} // namespace spieler::renderer