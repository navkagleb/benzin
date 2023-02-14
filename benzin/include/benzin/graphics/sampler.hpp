#pragma once

#include "benzin/graphics/shader.hpp"

namespace benzin
{

    struct Sampler
    {
        enum class TextureFilterType : uint8_t
        {
            Point = 0,
            Linear,
            Anisotropic
        };

        enum class TextureAddressMode : uint8_t
        {
            Wrap = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            Mirror = D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
            Clamp = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            Border = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            MirrorOnce = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE
        };

        TextureFilterType Minification{ TextureFilterType::Point };
        TextureFilterType Magnification{ TextureFilterType::Point };
        TextureFilterType MipLevel{ TextureFilterType::Point };
        TextureAddressMode AddressU{ TextureAddressMode::Wrap };
        TextureAddressMode AddressV{ TextureAddressMode::Wrap };
        TextureAddressMode AddressW{ TextureAddressMode::Wrap };

        static const Sampler& GetPointWrap();
        static const Sampler& GetPointClamp();
        static const Sampler& GetLinearWrap();
        static const Sampler& GetLinearClamp();
        static const Sampler& GetAnisotropicWrap();
        static const Sampler& GetAnisotropicClamp();

        static Sampler Get(TextureFilterType textureFilter, TextureAddressMode textureAddressMode);
    };

    struct StaticSampler
    {
        enum class BorderColor : uint8_t
        {
            TransparentBlack = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
            OpaqueBlack = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
            OpaqueWhite = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
        };

        Sampler Sampler;
        BorderColor BorderColor{ BorderColor::TransparentBlack };
        float MipLODBias{ 0.0f };
        uint32_t MaxAnisotropy{ 1 };
        ComparisonFunction ComparisonFunction{ ComparisonFunction::Always };
        Shader::Register ShaderRegister;
        ShaderVisibility ShaderVisibility{ ShaderVisibility::All };

        static StaticSampler GetPointWrap(const Shader::Register& shaderRegister);
        static StaticSampler GetPointClamp(const Shader::Register& shaderRegister);
        static StaticSampler GetLinearWrap(const Shader::Register& shaderRegister);
        static StaticSampler GetLinearClamp(const Shader::Register& shaderRegister);
        static StaticSampler GetAnisotropicWrap(const Shader::Register& shaderRegister);
        static StaticSampler GetAnisotropicClamp(const Shader::Register& shaderRegister);
        static StaticSampler GetDefaultForShadow(const Shader::Register& shaderRegister);
    };

} // namespace benzin
