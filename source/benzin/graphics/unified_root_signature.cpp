#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/unified_root_signature.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/sampler.hpp"

namespace benzin
{

    static auto CreateUnifiedD3D12RootParamers()
    {
        std::array<D3D12_ROOT_PARAMETER1, magic_enum::enum_count<UnifiedRootParameter>()> d3d12RootParamers;

        uint32_t constantBufferSpaceIndex = 0;

        d3d12RootParamers[+UnifiedRootParameter::RootConstantBuffer] = D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants
            {
                .ShaderRegister = 0,
                .RegisterSpace = constantBufferSpaceIndex++,
                .Num32BitValues = 32,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        d3d12RootParamers[+UnifiedRootParameter::FrameConstantBuffer] = D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor
            {
                .ShaderRegister = 0,
                .RegisterSpace = constantBufferSpaceIndex++,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        d3d12RootParamers[+UnifiedRootParameter::RenderPassConstantBuffer] = D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor
            {
                .ShaderRegister = 0,
                .RegisterSpace = constantBufferSpaceIndex++,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        d3d12RootParamers[+UnifiedRootParameter::TopLevelAs] = D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor
            {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        return d3d12RootParamers;
    }

    static D3D12_FILTER ToD3D12TextureFilter(const TextureFilterFunction& function, const TextureFilterType& type)
    {
        switch (function)
        {
            case TextureFilterFunction::Average:
            {
                switch (type)
                {
                    case TextureFilterType::Point: return D3D12_FILTER_MIN_MAG_MIP_POINT;
                    case TextureFilterType::Linear: return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                    case TextureFilterType::Anisotropic: return D3D12_FILTER_ANISOTROPIC;
                }

                std::unreachable();
            }
            case TextureFilterFunction::Min:
            {
                switch (type)
                {
                    case TextureFilterType::Point: return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT;
                    case TextureFilterType::Linear: return D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR;
                    case TextureFilterType::Anisotropic: return D3D12_FILTER_MINIMUM_ANISOTROPIC;
                }

                std::unreachable();
            }
            case TextureFilterFunction::Max:
            {
                switch (type)
                {
                    case TextureFilterType::Point: return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;
                    case TextureFilterType::Linear: return D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;
                    case TextureFilterType::Anisotropic: return D3D12_FILTER_MAXIMUM_ANISOTROPIC;
                }

                std::unreachable();
            }
        }

        std::unreachable();
    }

    static D3D12_STATIC_SAMPLER_DESC ToD3D12StaticSamplerDesc(const StaticSampler& staticSampler)
    {
        return D3D12_STATIC_SAMPLER_DESC
        {
            .Filter = ToD3D12TextureFilter(staticSampler.Sampler.FilterFunction, staticSampler.Sampler.FilterType),
            .AddressU = (D3D12_TEXTURE_ADDRESS_MODE)staticSampler.Sampler.AddressMode,
            .AddressV = (D3D12_TEXTURE_ADDRESS_MODE)staticSampler.Sampler.AddressMode,
            .AddressW = (D3D12_TEXTURE_ADDRESS_MODE)staticSampler.Sampler.AddressMode,
            .MipLODBias = staticSampler.MipLodBias,
            .MaxAnisotropy = staticSampler.MaxAnisotropy,
            .ComparisonFunc = (D3D12_COMPARISON_FUNC)staticSampler.ComparisonFunction,
            .BorderColor = (D3D12_STATIC_BORDER_COLOR)staticSampler.BorderColor,
            .MinLOD = 0.0f,
            .MaxLOD = D3D12_FLOAT32_MAX,
            .ShaderRegister = staticSampler.ShaderRegister.Index,
            .RegisterSpace = staticSampler.ShaderRegister.Space,
            .ShaderVisibility = (D3D12_SHADER_VISIBILITY)staticSampler.ShaderVisibility,
        };
    }

    //

    UnifiedRootSignature::UnifiedRootSignature(Device& device)
    {
        const auto d3d12RootParameters = CreateUnifiedD3D12RootParamers();

        uint32_t samplerSpaceIndex = 0;
        const auto d3d12StaticSamplerDescs = std::to_array(
        {
            ToD3D12StaticSamplerDesc(StaticSampler::GetPointWrap({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetPointClamp({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetLinearWrap({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetLinearClamp({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetAnisotropicWrap({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetAnisotropicClamp({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetMinLinearClamp({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetMaxLinearClamp({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler
            {
                .Sampler
                {
                    .FilterType = TextureFilterType::Point,
                    .AddressMode = TextureAddressMode::Border,
                },
                .BorderColor = TextureBorderColor::TransparentBlack,
                .ShaderRegister{ 0, samplerSpaceIndex++ },
            }),
        });

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12RootSignatureDesc
        {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1
            {
                .NumParameters = (uint32_t)d3d12RootParameters.size(),
                .pParameters = d3d12RootParameters.data(),
                .NumStaticSamplers = (uint32_t)d3d12StaticSamplerDescs.size(),
                .pStaticSamplers = d3d12StaticSamplerDescs.data(),
                .Flags
                {
                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
                },
            }
        };

        ComPtr<ID3DBlob> d3d12Blob;
        ComPtr<ID3DBlob> d3d12Error;
        const HRESULT hr = ::D3D12SerializeVersionedRootSignature(&d3d12RootSignatureDesc, &d3d12Blob, &d3d12Error);
        if (FAILED(hr))
        {
            BenzinEnsure(hr, "Failed to Serialize RootSignature. Error: {}", (const char*)d3d12Error->GetBufferPointer());
        }

        BenzinEnsure(device.GetD3D12Device()->CreateRootSignature(
            0,
            d3d12Blob->GetBufferPointer(),
            d3d12Blob->GetBufferSize(),
            IID_PPV_ARGS(&m_D3D12RootSignature)
        ));

        SetDxObjectDebugName(m_D3D12RootSignature, "UnifiedRootSignature");
    }

    UnifiedRootSignature::~UnifiedRootSignature()
    {
        BenzinSafeDxObjectRelease(m_D3D12RootSignature);
    }

}
