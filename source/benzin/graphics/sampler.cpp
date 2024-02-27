#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/sampler.hpp"

namespace benzin
{

    static constexpr Sampler g_PointWrapSampler
    {
        .Minification = TextureFilterType::Point,
        .Magnification = TextureFilterType::Point,
        .MipLevel = TextureFilterType::Point,
        .AddressU = TextureAddressMode::Wrap,
        .AddressV = TextureAddressMode::Wrap,
        .AddressW = TextureAddressMode::Wrap,
    };

    static constexpr Sampler g_PointClampSampler
    {
        .Minification = TextureFilterType::Point,
        .Magnification = TextureFilterType::Point,
        .MipLevel = TextureFilterType::Point,
        .AddressU = TextureAddressMode::Clamp,
        .AddressV = TextureAddressMode::Clamp,
        .AddressW = TextureAddressMode::Clamp,
    };

    static constexpr Sampler g_LinearWrapSampler
    {
        .Minification = TextureFilterType::Linear,
        .Magnification = TextureFilterType::Linear,
        .MipLevel = TextureFilterType::Linear,
        .AddressU = TextureAddressMode::Wrap,
        .AddressV = TextureAddressMode::Wrap,
        .AddressW = TextureAddressMode::Wrap,
    };

    static constexpr Sampler g_LinearClampSampler
    {
        .Minification = TextureFilterType::Linear,
        .Magnification = TextureFilterType::Linear,
        .MipLevel = TextureFilterType::Linear,
        .AddressU = TextureAddressMode::Clamp,
        .AddressV = TextureAddressMode::Clamp,
        .AddressW = TextureAddressMode::Clamp,
    };

    static constexpr Sampler g_AnisotropicWrapSampler
    {
        .Minification = TextureFilterType::Anisotropic,
        .Magnification = TextureFilterType::Anisotropic,
        .MipLevel = TextureFilterType::Anisotropic,
        .AddressU = TextureAddressMode::Clamp,
        .AddressV = TextureAddressMode::Clamp,
        .AddressW = TextureAddressMode::Clamp,
    };

    static constexpr Sampler g_AnisotropicClampSampler
    {
        .Minification = TextureFilterType::Anisotropic,
        .Magnification = TextureFilterType::Anisotropic,
        .MipLevel = TextureFilterType::Anisotropic,
        .AddressU = TextureAddressMode::Clamp,
        .AddressV = TextureAddressMode::Clamp,
        .AddressW = TextureAddressMode::Clamp,
    };

    // Sampler

    const Sampler& Sampler::GetPointWrap()
    {
        return g_PointWrapSampler;
    }

    const Sampler& Sampler::GetPointClamp()
    {
        return g_PointClampSampler;
    }

    const Sampler& Sampler::GetLinearWrap()
    {
        return g_LinearWrapSampler;
    }

    const Sampler& Sampler::GetLinearClamp()
    {
        return g_LinearClampSampler;
    }

    const Sampler& Sampler::GetAnisotropicWrap()
    {
        return g_AnisotropicWrapSampler;
    }

    const Sampler& Sampler::GetAnisotropicClamp()
    {
        return g_AnisotropicClampSampler;
    }

    Sampler Sampler::Get(TextureFilterType textureFilter, TextureAddressMode textureAddressMode)
    {
        return Sampler
        {
            .Minification = textureFilter,
            .Magnification = textureFilter,
            .MipLevel = textureFilter,
            .AddressU = textureAddressMode,
            .AddressV = textureAddressMode,
            .AddressW = textureAddressMode,
        };
    }

    // StaticSampler

    StaticSampler StaticSampler::GetPointWrap(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler = g_PointWrapSampler,
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetPointClamp(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler = g_PointClampSampler,
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetLinearWrap(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler = g_LinearWrapSampler,
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetLinearClamp(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler = g_LinearClampSampler,
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetAnisotropicWrap(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler = g_AnisotropicWrapSampler,
            .MaxAnisotropy = 16,
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetAnisotropicClamp(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler = g_AnisotropicClampSampler,
            .MaxAnisotropy = 16,
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetDefaultForShadow(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .Minification = TextureFilterType::Linear,
                .Magnification = TextureFilterType::Linear,
                .MipLevel = TextureFilterType::Linear,
                .AddressU = TextureAddressMode::Border,
                .AddressV = TextureAddressMode::Border,
                .AddressW = TextureAddressMode::Border,
            },
            .BorderColor = TextureBorderColor::OpaqueBlack,
            .MaxAnisotropy = 16,
            .ComparisonFunction = ComparisonFunction::LessEqual,
            .ShaderRegister = shaderRegister,
        };
    }

} // namespace benzin
