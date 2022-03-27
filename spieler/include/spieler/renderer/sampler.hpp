#pragma once

#include "renderer_object.hpp"
#include "shader_visibility.hpp"

namespace Spieler
{

    class DescriptorHeap;

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
        Sampler(const SamplerProps& props, const DescriptorHeap& descriptorHeap, std::uint32_t descriptorHeapIndex);

    public:
        void Init(const SamplerProps& props, const DescriptorHeap& descriptorHeap, std::uint32_t descriptorHeapIndex);

        void Bind(std::uint32_t rootParameterIndex) const;

    private:
        SamplerProps m_Props;
        const DescriptorHeap* m_DescriptorHeap{ nullptr };
        std::uint32_t m_DescriptorHeapIndex{ 0 };
    };

    struct StaticSampler
    {
        enum BorderColor
        {
            BorderColor_TransparentBlack = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
            BorderColor_OpaqueBlack = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
            BorderColor_OpaqueWhite = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
        };

        TextureFilter TextureFilter;
        TextureAddressMode AddressU{ TextureAddressMode_Wrap };
        TextureAddressMode AddressV{ TextureAddressMode_Wrap };
        TextureAddressMode AddressW{ TextureAddressMode_Wrap };
        BorderColor BorderColor{ BorderColor_TransparentBlack };
        std::uint32_t ShaderRegister{ 0 };
        std::uint32_t RegisterSpace{ 0 };
        ShaderVisibility ShaderVisibility{ ShaderVisibility_All };

        StaticSampler() = default;

        StaticSampler(struct TextureFilter textureFilter, TextureAddressMode textureAddressMode, std::uint32_t shaderRegister = 0)
            : TextureFilter(textureFilter)
            , AddressU(textureAddressMode)
            , AddressV(textureAddressMode)
            , AddressW(textureAddressMode)
            , ShaderRegister(shaderRegister)
        {}

        StaticSampler(struct TextureFilter textureFilter, TextureAddressMode addressU, TextureAddressMode addressV, TextureAddressMode addressW, std::uint32_t shaderRegister = 0)
            : TextureFilter(textureFilter)
            , AddressU(addressU)
            , AddressV(addressV)
            , AddressW(addressW)
            , ShaderRegister(shaderRegister)
        {}
    };

} // namespace Spieler