#pragma once

#include "spieler/graphics/common.hpp"

namespace spieler
{

    class DescriptorManager;

    enum class TextureFilterType : uint8_t
    {
        Point = 0,
        Linear,
        Anisotropic
    };

    struct TextureFilter
    {
        TextureFilterType Minification{ TextureFilterType::Point };
        TextureFilterType Magnification{ TextureFilterType::Point };
        TextureFilterType MipLevel{ TextureFilterType::Point };

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

    enum class TextureAddressMode : uint8_t
    {
        Wrap = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        Mirror = D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        Clamp = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        Border = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        MirrorOnce = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE
    };

    struct SamplerConfig
    {
        TextureFilter TextureFilter;
        TextureAddressMode AddressU{ TextureAddressMode::Wrap };
        TextureAddressMode AddressV{ TextureAddressMode::Wrap };
        TextureAddressMode AddressW{ TextureAddressMode::Wrap };

        SamplerConfig() = default;

        SamplerConfig(struct TextureFilter textureFilter, TextureAddressMode textureAddressMode)
            : TextureFilter(textureFilter)
            , AddressU(textureAddressMode)
            , AddressV(textureAddressMode)
            , AddressW(textureAddressMode)
        {}
      
        SamplerConfig(struct TextureFilter textureFilter, TextureAddressMode addressU, TextureAddressMode addressV, TextureAddressMode addressW)
            : TextureFilter(textureFilter)
            , AddressU(addressU)
            , AddressV(addressV)
            , AddressW(addressW)
        {}
    };

    struct StaticSampler
    {
        enum class BorderColor : uint8_t
        {
            TransparentBlack = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
            OpaqueBlack = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
            OpaqueWhite = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
        };

        TextureFilter TextureFilter;
        TextureAddressMode AddressU{ TextureAddressMode::Wrap };
        TextureAddressMode AddressV{ TextureAddressMode::Wrap };
        TextureAddressMode AddressW{ TextureAddressMode::Wrap };
        BorderColor BorderColor{ BorderColor::TransparentBlack };
        uint32_t ShaderRegister{ 0 };
        uint32_t RegisterSpace{ 0 };
        ShaderVisibility ShaderVisibility{ ShaderVisibility::All };

        StaticSampler() = default;

        StaticSampler(struct TextureFilter textureFilter, TextureAddressMode textureAddressMode, uint32_t shaderRegister = 0)
            : TextureFilter(textureFilter)
            , AddressU(textureAddressMode)
            , AddressV(textureAddressMode)
            , AddressW(textureAddressMode)
            , ShaderRegister(shaderRegister)
        {}

        StaticSampler(struct TextureFilter textureFilter, TextureAddressMode addressU, TextureAddressMode addressV, TextureAddressMode addressW, uint32_t shaderRegister = 0)
            : TextureFilter(textureFilter)
            , AddressU(addressU)
            , AddressV(addressV)
            , AddressW(addressW)
            , ShaderRegister(shaderRegister)
        {}
    };

} // namespace spieler