#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/texture.hpp"

namespace benzin
{

    static uint32_t g_TextureResourceCounter = 0;

    TextureResource::TextureResource(ID3D12Resource* d3d12Resource, const Config& config)
        : Resource{ d3d12Resource }
        , m_Config{ config }
    {
        SetName("TextureResource" + std::to_string(g_TextureResourceCounter));

        BENZIN_INFO("{} created", GetName());

        g_TextureResourceCounter++;
    }

    uint32_t TextureResource::PushRenderTargetView(const Descriptor& rtv)
    {
        return PushResourceView(Descriptor::Type::RenderTargetView, rtv);
    }

    uint32_t TextureResource::PushDepthStencilView(const Descriptor& dsv)
    {
        return PushResourceView(Descriptor::Type::DepthStencilView, dsv);
    }

    Descriptor TextureResource::GetRenderTargetView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::RenderTargetView, index);
    }

    Descriptor TextureResource::GetDepthStencilView(uint32_t index) const
    {
        return GetResourceView(Descriptor::Type::DepthStencilView, index);
    }

} // namespace benzin
