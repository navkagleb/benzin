#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/sampler.hpp"

namespace benzin
{

    StaticSampler StaticSampler::GetPointWrap(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .FilterType = TextureFilterType::Point,
                .AddressMode = TextureAddressMode::Wrap,
            },
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetPointClamp(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .FilterType = TextureFilterType::Point,
                .AddressMode = TextureAddressMode::Clamp,
            },
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetLinearWrap(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .FilterType = TextureFilterType::Linear,
                .AddressMode = TextureAddressMode::Wrap,
            },
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetLinearClamp(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .FilterType = TextureFilterType::Linear,
                .AddressMode = TextureAddressMode::Clamp,
            },
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetAnisotropicWrap(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .FilterType = TextureFilterType::Anisotropic,
                .AddressMode = TextureAddressMode::Wrap,
            },
            .MaxAnisotropy = 16,
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetAnisotropicClamp(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .FilterType = TextureFilterType::Anisotropic,
                .AddressMode = TextureAddressMode::Clamp,
            },
            .MaxAnisotropy = 16,
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetMinLinearClamp(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .FilterFunction = TextureFilterFunction::Min,
                .FilterType = TextureFilterType::Linear,
                .AddressMode = TextureAddressMode::Clamp,
            },
            .ShaderRegister = shaderRegister,
        };
    }

    StaticSampler StaticSampler::GetMaxLinearClamp(const struct ShaderRegister& shaderRegister)
    {
        return StaticSampler
        {
            .Sampler
            {
                .FilterFunction = TextureFilterFunction::Max,
                .FilterType = TextureFilterType::Linear,
                .AddressMode = TextureAddressMode::Clamp,
            },
            .ShaderRegister = shaderRegister,
        };
    }

} // namespace benzin
