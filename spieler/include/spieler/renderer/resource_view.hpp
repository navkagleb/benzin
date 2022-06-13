#pragma once

#include "spieler/renderer/descriptor_manager.hpp"

namespace spieler::renderer
{

    class Texture2DResource;
    class Device;
    class Context;
    struct SamplerConfig;
    class ConstantBuffer;

    template <typename Descriptor>
    class ResourceView
    {
    public:
        virtual ~ResourceView() = default;

    public:
        const Descriptor& GetDescriptor() const { return m_Descriptor; }

    public:
        explicit operator bool() const { return m_Descriptor.operator bool(); }

    protected:
        Descriptor m_Descriptor;
    };

    class RenderTargetView : public ResourceView<RTVDescriptor>
    {
    public:
        RenderTargetView() = default;
        RenderTargetView(Device& device, const Texture2DResource& texture);

    public:
        void Init(Device& device, const Texture2DResource& texture);
    };

    class DepthStencilView : public ResourceView<DSVDescriptor>
    {
    public:
        DepthStencilView() = default;
        DepthStencilView(Device& device, const Texture2DResource& texture);

    public:
        void Init(Device& device, const Texture2DResource& texture);
    };

    class SamplerView : public ResourceView<SamplerDescriptor>
    {
    public:
        void Init(Device& device, DescriptorManager& descriptorManager, const SamplerConfig& samplerConfig);
        void Bind(Context& context, uint32_t rootParameterIndex) const;
    };

#if 0
    class ConstantBufferView : public ResourceView<CBVDescriptor>
    {
    public:
        void Init(Device& device, const ConstantBuffer& resource);
        void Bind(Context& context, U32 rootParameterIndex) const;
    };
#endif

    class ShaderResourceView : public ResourceView<SRVDescriptor>
    {
    public:
        ShaderResourceView() = default;
        ShaderResourceView(Device& device, const Texture2DResource& texture);

    public:
        void Init(Device& device, const Texture2DResource& texture);
        void Bind(Context& context, uint32_t rootParameterIndex) const;
    };

    class UnorderedAccessView : public ResourceView<UAVDescriptor>
    {
    public:
        UnorderedAccessView() = default;
        UnorderedAccessView(Device& device, const Texture2DResource& texture);

    public:
        void Init(Device& device, const Texture2DResource& texture);
        void Bind(Context& context, uint32_t rootParameterIndex) const;
    };

} // namespace spieler::renderer