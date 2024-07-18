#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    enum class TextureFilterFunction : uint8_t
    {
        Average,
        Min,
        Max,
    };

    enum class TextureFilterType : uint8_t
    {
        Point,
        Linear,
        Anisotropic,
    };

    enum class TextureAddressMode : std::underlying_type_t<D3D12_TEXTURE_ADDRESS_MODE>
    {
        Wrap = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        Mirror = D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        Clamp = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        Border = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        MirrorOnce = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE,
    };

    struct Sampler
    {
        TextureFilterFunction FilterFunction = TextureFilterFunction::Average;
        TextureFilterType FilterType = TextureFilterType::Point;
        TextureAddressMode AddressMode = TextureAddressMode::Wrap;
    };

    enum class TextureBorderColor : std::underlying_type_t<D3D12_STATIC_BORDER_COLOR>
    {
        TransparentBlack = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
        OpaqueBlack = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
        OpaqueWhite = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
    };

    struct StaticSampler
    {
        Sampler Sampler;
        TextureBorderColor BorderColor = TextureBorderColor::TransparentBlack;
        float MipLodBias = 0.0f;
        uint32_t MaxAnisotropy = 1;
        ComparisonFunction ComparisonFunction = ComparisonFunction::Always;
        ShaderRegister ShaderRegister;
        ShaderVisibility ShaderVisibility = ShaderVisibility::All;

        static StaticSampler GetPointWrap(const struct ShaderRegister& shaderRegister);
        static StaticSampler GetPointClamp(const struct ShaderRegister& shaderRegister);
        static StaticSampler GetLinearWrap(const struct ShaderRegister& shaderRegister);
        static StaticSampler GetLinearClamp(const struct ShaderRegister& shaderRegister);
        static StaticSampler GetAnisotropicWrap(const struct ShaderRegister& shaderRegister);
        static StaticSampler GetAnisotropicClamp(const struct ShaderRegister& shaderRegister);

        static StaticSampler GetMinLinearClamp(const struct ShaderRegister& shaderRegister);
        static StaticSampler GetMaxLinearClamp(const struct ShaderRegister& shaderRegister);
    };

} // namespace benzin
