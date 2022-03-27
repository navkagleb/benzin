#include "renderer/root_signature.hpp"

#include "renderer/sampler.hpp"

namespace Spieler
{

    namespace _Internal
    {

        extern D3D12_FILTER ToD3D12TextureFilter(const TextureFilter& filter);

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
                    case RootParameterType_DescriptorTable:
                    {
                        const auto& descriptorTable{ std::get<RootDescriptorTable>(rootParameter.Child) };
                        d3d12RootParameter.DescriptorTable.NumDescriptorRanges = descriptorTable.DescriptorRanges.size();
                        d3d12RootParameter.DescriptorTable.pDescriptorRanges = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE*>(descriptorTable.DescriptorRanges.data());

                        break;
                    }

                    case RootParameterType_32BitConstants:
                    {
                        const auto& rootConstants{ std::get<RootConstants>(rootParameter.Child) };
                        d3d12RootParameter.Constants.Num32BitValues = rootConstants.Count;
                        d3d12RootParameter.Constants.ShaderRegister = rootConstants.ShaderRegister;
                        d3d12RootParameter.Constants.RegisterSpace = rootConstants.RegisterSpace;

                        break;
                    }

                    case RootParameterType_ConstantBufferView:
                    case RootParameterType_ShaderResourceView:
                    case RootParameterType_UnorderedAccessView:
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
                D3D12_STATIC_SAMPLER_DESC d3d12StaticSamplerDesc{};
                d3d12StaticSamplerDesc.Filter = ToD3D12TextureFilter(staticSampler.TextureFilter);
                d3d12StaticSamplerDesc.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressU);
                d3d12StaticSamplerDesc.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressV);
                d3d12StaticSamplerDesc.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressW);
                d3d12StaticSamplerDesc.MipLODBias = 0.0f;
                d3d12StaticSamplerDesc.MaxAnisotropy = 1.0f;
                d3d12StaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
                d3d12StaticSamplerDesc.BorderColor = static_cast<D3D12_STATIC_BORDER_COLOR>(staticSampler.BorderColor);
                d3d12StaticSamplerDesc.MinLOD = 0.0f;
                d3d12StaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
                d3d12StaticSamplerDesc.ShaderRegister = static_cast<UINT>(staticSampler.ShaderRegister);
                d3d12StaticSamplerDesc.RegisterSpace = static_cast<UINT>(staticSampler.RegisterSpace);
                d3d12StaticSamplerDesc.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(staticSampler.ShaderVisibility);

                d3d12StaticSamplers.push_back(d3d12StaticSamplerDesc);
            }

            return d3d12StaticSamplers;
        }

    } // namespace _Internal

    bool RootSignature::Init(const std::vector<RootParameter>& rootParameters)
    {
        SPIELER_ASSERT(!rootParameters.empty());

        const std::vector<D3D12_ROOT_PARAMETER> d3d12RootParameters{ _Internal::ToD3D12RootParameters(rootParameters) };

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.NumParameters = static_cast<UINT>(d3d12RootParameters.size());
        rootSignatureDesc.pParameters = d3d12RootParameters.data();
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        SPIELER_RETURN_IF_FAILED(Init(rootSignatureDesc));

        return true;
    }

    bool RootSignature::Init(const std::vector<RootParameter>& rootParameters, const std::vector<StaticSampler>& staticSamplers)
    {
        SPIELER_ASSERT(!rootParameters.empty());
        SPIELER_ASSERT(!staticSamplers.empty());

        const std::vector<D3D12_ROOT_PARAMETER> d3d12RootParameters{ _Internal::ToD3D12RootParameters(rootParameters) };
        const std::vector<D3D12_STATIC_SAMPLER_DESC> d3d12StaticSamplers{ _Internal::ToD3d12StaticSamplers(staticSamplers) };

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.NumParameters = static_cast<UINT>(d3d12RootParameters.size());
        rootSignatureDesc.pParameters = d3d12RootParameters.data();
        rootSignatureDesc.NumStaticSamplers = static_cast<UINT>(d3d12StaticSamplers.size());
        rootSignatureDesc.pStaticSamplers = d3d12StaticSamplers.data();
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        SPIELER_RETURN_IF_FAILED(Init(rootSignatureDesc));
        
        return true;
    }

    bool RootSignature::Init(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc)
    {
        ComPtr<ID3DBlob> serializedRootSignature;
        ComPtr<ID3DBlob> error;

        const HRESULT result = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &serializedRootSignature,
            &error
        );

        if (error)
        {
            ::OutputDebugString(reinterpret_cast<const char*>(error->GetBufferPointer()));
            return false;
        }

        SPIELER_RETURN_IF_FAILED(result);

        SPIELER_RETURN_IF_FAILED(GetDevice()->CreateRootSignature(
            0,
            serializedRootSignature->GetBufferPointer(),
            serializedRootSignature->GetBufferSize(),
            __uuidof(ID3D12RootSignature),
            &m_Handle
        ));

        return true;
    }

    void RootSignature::Bind() const
    {
        SPIELER_ASSERT(m_Handle);

        GetCommandList()->SetGraphicsRootSignature(m_Handle.Get());
    }

} // namespace Spieler