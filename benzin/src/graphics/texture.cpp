#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/texture.hpp"

namespace benzin
{

    TextureResource::TextureResource(ID3D12Resource* d3d12Resource, const Config& config, std::string_view debugName)
        : Resource{ d3d12Resource }
        , m_Config{ config }
    {
        SetDebugName(debugName, true);
    }

    uint32_t TextureResource::PushRenderTargetView(const Descriptor& rtv)
    {
        return PushResourceView(Descriptor::Type::RenderTargetView, rtv);
    }

    uint32_t TextureResource::PushDepthStencilView(const Descriptor& dsv)
    {
        return PushResourceView(Descriptor::Type::DepthStencilView, dsv);
    }

    const Descriptor& TextureResource::GetRenderTargetView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::RenderTargetView, index);
    }

    const Descriptor& TextureResource::GetDepthStencilView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::DepthStencilView, index);
    }

} // namespace benzin
