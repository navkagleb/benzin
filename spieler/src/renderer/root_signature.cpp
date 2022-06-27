#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/root_signature.hpp"

#include "spieler/core/common.hpp"
#include "spieler/core/logger.hpp"

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/sampler.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        // Defined in resource_view.cpp
        extern D3D12_FILTER ConvertToD3D12TextureFilter(const TextureFilter& filter);

        std::vector<D3D12_ROOT_PARAMETER> ToD3D12RootParameters(const std::vector<RootParameter>& rootParameters)
        {
            std::vector<D3D12_ROOT_PARAMETER> d3d12RootParameters;
            d3d12RootParameters.reserve(rootParameters.size());

            for (const auto& rootParameter : rootParameters)
            {
                D3D12_ROOT_PARAMETER d3d12RootParameter{};
                d3d12RootParameter.ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(rootParameter.Type);
                d3d12RootParameter.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(rootParameter.ShaderVisibility);

                switch (rootParameter.Type)
                {
                    case RootParameterType::DescriptorTable:
                    {
                        const auto& descriptorTable{ std::get<RootDescriptorTable>(rootParameter.Child) };
                        d3d12RootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(descriptorTable.DescriptorRanges.size());
                        d3d12RootParameter.DescriptorTable.pDescriptorRanges = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE*>(descriptorTable.DescriptorRanges.data());

                        break;
                    }
                    case RootParameterType::_32BitConstants:
                    {
                        const auto& rootConstants{ std::get<RootConstants>(rootParameter.Child) };
                        d3d12RootParameter.Constants.Num32BitValues = rootConstants.Count;
                        d3d12RootParameter.Constants.ShaderRegister = rootConstants.ShaderRegister;
                        d3d12RootParameter.Constants.RegisterSpace = rootConstants.RegisterSpace;

                        break;
                    }
                    case RootParameterType::ConstantBufferView:
                    case RootParameterType::ShaderResourceView:
                    case RootParameterType::UnorderedAccessView:
                    {
                        const auto& rootDescriptor{ std::get<RootDescriptor>(rootParameter.Child) };
                        d3d12RootParameter.Descriptor.ShaderRegister = rootDescriptor.ShaderRegister;
                        d3d12RootParameter.Descriptor.RegisterSpace = rootDescriptor.RegisterSpace;

                        break;
                    }
                    default:
                    {
                        SPIELER_ASSERT(false);

                        break;
                    }
                }

                d3d12RootParameters.push_back(d3d12RootParameter);
            }

            return d3d12RootParameters;
        }

        std::vector<D3D12_STATIC_SAMPLER_DESC> ToD3d12StaticSamplers(const std::vector<StaticSampler>& staticSamplers)
        {
            std::vector<D3D12_STATIC_SAMPLER_DESC> d3d12StaticSamplers;
            d3d12StaticSamplers.reserve(staticSamplers.size());

            for (const StaticSampler& staticSampler : staticSamplers)
            {
                d3d12StaticSamplers.push_back(D3D12_STATIC_SAMPLER_DESC
                {
                    .Filter = ConvertToD3D12TextureFilter(staticSampler.TextureFilter),
                    .AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressU),
                    .AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressV),
                    .AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressW),
                    .MipLODBias = 0.0f,
                    .MaxAnisotropy = 1,
                    .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
                    .BorderColor = static_cast<D3D12_STATIC_BORDER_COLOR>(staticSampler.BorderColor),
                    .MinLOD = 0.0f,
                    .MaxLOD = D3D12_FLOAT32_MAX,
                    .ShaderRegister = static_cast<UINT>(staticSampler.ShaderRegister),
                    .RegisterSpace = static_cast<UINT>(staticSampler.RegisterSpace),
                    .ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(staticSampler.ShaderVisibility)
                });
            }

            return d3d12StaticSamplers;
        }

    } // namespace _internal

    RootSignature::RootSignature(Device& device, const std::vector<RootParameter>& rootParameters)
    {
        SPIELER_ASSERT(Init(device, rootParameters));
    }

    RootSignature::RootSignature(Device& device, const std::vector<RootParameter>& rootParameters, const std::vector<StaticSampler>& staticSamplers)
    {
        SPIELER_ASSERT(Init(device, rootParameters, staticSamplers));
    }

    RootSignature::RootSignature(RootSignature&& other) noexcept
        : m_RootSignature{ std::exchange(other.m_RootSignature, nullptr) }
    {}

    bool RootSignature::Init(Device& device, const std::vector<RootParameter>& rootParameters)
    {
        SPIELER_ASSERT(!rootParameters.empty());

        const std::vector<D3D12_ROOT_PARAMETER> d3d12RootParameters{ _internal::ToD3D12RootParameters(rootParameters) };

        const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc
        {
            .NumParameters = static_cast<UINT>(d3d12RootParameters.size()),
            .pParameters = d3d12RootParameters.data(),
            .NumStaticSamplers = 0,
            .pStaticSamplers = nullptr,
            .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        };

        SPIELER_RETURN_IF_FAILED(Init(device, rootSignatureDesc));

        return true;
    }

    bool RootSignature::Init(Device& device, const std::vector<RootParameter>& rootParameters, const std::vector<StaticSampler>& staticSamplers)
    {
        SPIELER_ASSERT(!rootParameters.empty());
        SPIELER_ASSERT(!staticSamplers.empty());

        const std::vector<D3D12_ROOT_PARAMETER> d3d12RootParameters{ _internal::ToD3D12RootParameters(rootParameters) };
        const std::vector<D3D12_STATIC_SAMPLER_DESC> d3d12StaticSamplers{ _internal::ToD3d12StaticSamplers(staticSamplers) };

        const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc
        {
            .NumParameters = static_cast<UINT>(d3d12RootParameters.size()),
            .pParameters = d3d12RootParameters.data(),
            .NumStaticSamplers = static_cast<UINT>(d3d12StaticSamplers.size()),
            .pStaticSamplers = d3d12StaticSamplers.data(),
            .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        };

        SPIELER_RETURN_IF_FAILED(Init(device, rootSignatureDesc));
        
        return true;
    }

    bool RootSignature::Init(Device& device, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc)
    {
        ComPtr<ID3DBlob> serializedRootSignature;
        ComPtr<ID3DBlob> error;

        const ::HRESULT result
        {
            D3D12SerializeRootSignature(
                &rootSignatureDesc,
                D3D_ROOT_SIGNATURE_VERSION_1,
                &serializedRootSignature,
                &error
            )
        };

        if (error)
        {
            SPIELER_CRITICAL("{}", reinterpret_cast<const char*>(error->GetBufferPointer()));
            return false;
        }

        SPIELER_RETURN_IF_FAILED(result);

        SPIELER_RETURN_IF_FAILED(device.GetNativeDevice()->CreateRootSignature(
            0,
            serializedRootSignature->GetBufferPointer(),
            serializedRootSignature->GetBufferSize(),
            __uuidof(ID3D12RootSignature),
            &m_RootSignature
        ));

        return true;
    }

    RootSignature& RootSignature::operator=(RootSignature&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_RootSignature = std::exchange(other.m_RootSignature, nullptr);

        return *this;
    }

} // namespace spieler::renderer