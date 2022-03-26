#pragma once

#include <algorithm>

#include "renderer_object.hpp"

namespace Spieler
{

    const DescriptorHeap;

    enum TextureFilterType : std::uint8_t
    {
        TextureFilterType_Point = 0,
        TextureFilterType_Linear,
        TextureFilterType_Anisotropic
    };

    struct TextureFilter
    {
        TextureFilterType Minification{ TextureFilterType_Point };
        TextureFilterType Magnification{ TextureFilterType_Point };
        TextureFilterType MipLevel{ TextureFilterType_Point };

        TextureFilter() = default;

        TextureFilter(TextureFilterType filter)
            : Minification(filter)
            , Magnification(filter)
            , MipLevel(filter)
        {}

        TextureFilter(TextureFilterType minification, TextureFilterType magnification, TextureFilterType mipLevel)
            : Minification(minification)
            , Magnification(magnification)
            , MipLevel(mipLevel)
        {}
    };

    enum TextureAddressMode : std::uint32_t
    {
        TextureAddressMode_Wrap = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        TextureAddressMode_Mirror = D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        TextureAddressMode_Clamp = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        TextureAddressMode_Border = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        TextureAddressMode_MirrorOnce = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE
    };

    struct SamplerProps
    {
        TextureFilter TextureFilter;
        TextureAddressMode AddressU{ TextureAddressMode_Wrap };
        TextureAddressMode AddressV{ TextureAddressMode_Wrap };
        TextureAddressMode AddressW{ TextureAddressMode_Wrap };

        SamplerProps() = default;

        SamplerProps(struct TextureFilter textureFilter, TextureAddressMode textureAddressMode)
            : TextureFilter(textureFilter)
            , AddressU(textureAddressMode)
            , AddressV(textureAddressMode)
            , AddressW(textureAddressMode)
        {}
      
        SamplerProps(struct TextureFilter textureFilter, TextureAddressMode addressU, TextureAddressMode addressV, TextureAddressMode addressW)
            : TextureFilter(textureFilter)
            , AddressU(addressU)
            , AddressV(addressV)
            , AddressW(addressW)
        {}
    };

    class Sampler : public RendererObject
    {
    public:
        Sampler() = default;
        Sampler(const SamplerProps& props, const DescriptorHeap& heap, std::uint32_t index);

    public:
        void Init(const SamplerProps& props, const DescriptorHeap& heap, std::uint32_t index);

    private:
        SamplerProps m_Props;
    };

} // namespace Spieler