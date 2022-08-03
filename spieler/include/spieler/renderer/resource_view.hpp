#pragma once

#include "spieler/core/assert.hpp"

#include "spieler/renderer/descriptor_manager.hpp"
#include "spieler/renderer/resource.hpp"

namespace spieler::renderer
{

    class Device;
    class Resource;
    class BufferResource;

    template <typename Descriptor>
    class DescriptorView
    {
    public:
        const Descriptor& GetDescriptor() const { return m_Descriptor; }

    protected:
        Descriptor m_Descriptor;
    };

    class VertexBufferView
    {
    public:
        VertexBufferView() = default;
        VertexBufferView(const BufferResource& resource);

    private:
        D3D12_VERTEX_BUFFER_VIEW m_View{};
    };

    class IndexBufferView
    {
    public:
        IndexBufferView() = default;
        IndexBufferView(const BufferResource& resource);

    private:
        D3D12_INDEX_BUFFER_VIEW m_View{};
    };

    class RenderTargetView final : public DescriptorView<RTVDescriptor>
    {
    public:
        RenderTargetView() = default;
        RenderTargetView(Device& device, const Resource& resource);
    };

    class DepthStencilView final : public DescriptorView<DSVDescriptor>
    {
    public:
        DepthStencilView() = default;
        DepthStencilView(Device& device, const Resource& resource);
    };

    struct SamplerConfig;

    class SamplerView final : public DescriptorView<SamplerDescriptor>
    {
    public:
        SamplerView() = default;
        SamplerView(Device& device, const SamplerConfig& samplerConfig);
    };

    class ConstantBufferView final : public DescriptorView<CBVDescriptor>
    {
    public:
        ConstantBufferView() = default;
        //ConstantBufferView(Device& device, const Resource& resource);
    };

    class ShaderResourceView final : public DescriptorView<SRVDescriptor>
    {
    public:
        ShaderResourceView() = default;
        ShaderResourceView(Device& device, const Resource& resource);
    };

    class UnorderedAccessView final : public DescriptorView<UAVDescriptor>
    {
    public:
        UnorderedAccessView() = default;
        UnorderedAccessView(Device& device, const Resource& resource);
    };

    namespace concepts
    {

        template <typename View>
        concept BufferView = std::is_same_v<View, VertexBufferView> || std::is_same_v<View, IndexBufferView>;

        template <typename View>
        concept DescriptorView =
            std::is_same_v<View, RenderTargetView> ||
            std::is_same_v<View, DepthStencilView> ||
            std::is_same_v<View, ConstantBufferView> ||
            std::is_same_v<View, ShaderResourceView> ||
            std::is_same_v<View, UnorderedAccessView>;

        template <typename View>
        concept ResourceView = BufferView<View> || DescriptorView<View>;

    } // namespace concepts

    class ViewContainer
    {
    private:
        using ViewVariant = std::variant<VertexBufferView, IndexBufferView, RenderTargetView, DepthStencilView, ConstantBufferView, ShaderResourceView, UnorderedAccessView>;

    public:
        ViewContainer(Resource& resource);

    public:
        template <concepts::ResourceView View>
        bool HasView() const;

        template <concepts::ResourceView View>
        const View& GetView() const;

        template <concepts::BufferView View>
        void CreateView();

        template <concepts::DescriptorView View>
        void CreateView(Device& device);

        void Clear();

    private:
        Resource& m_Resource;
        std::unordered_map<uint64_t, ViewVariant> m_Views;
    };

} // namespace spieler::renderer

#include "spieler/renderer/resource_view.inl"
