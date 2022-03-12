#include "root_signature.h"

#include <d3dx12.h>

namespace Spieler
{

    bool RootSignature::Init(const std::vector<RootParameter>& rootParameters)
    {
        std::vector<D3D12_ROOT_PARAMETER> d3d12RootParameters;
        d3d12RootParameters.reserve(rootParameters.size());

        for (const auto& rootParameter : rootParameters)
        {
            D3D12_ROOT_PARAMETER d3d12RootParameter{};
            d3d12RootParameter.ParameterType    = static_cast<D3D12_ROOT_PARAMETER_TYPE>(rootParameter.Type);
            d3d12RootParameter.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(rootParameter.ShaderVisibility);

            switch (rootParameter.Type)
            {
                case RootParameterType_DescriptorTable:
                {
                    const auto& descriptorTable = std::get<RootDescriptorTable>(rootParameter.Child);

                    d3d12RootParameter.DescriptorTable.NumDescriptorRanges  = descriptorTable.DescriptorRanges.size();
                    d3d12RootParameter.DescriptorTable.pDescriptorRanges    = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE*>(descriptorTable.DescriptorRanges.data());

                    break;
                }

                case RootParameterType_32BitConstants:
                {
                    const auto& rootConstants = std::get<RootConstants>(rootParameter.Child);

                    d3d12RootParameter.Constants.Num32BitValues = rootConstants.Count;
                    d3d12RootParameter.Constants.ShaderRegister = rootConstants.ShaderRegister;
                    d3d12RootParameter.Constants.RegisterSpace  = rootConstants.RegisterSpace;

                    break;
                }

                case RootParameterType_ConstantBufferView:
                case RootParameterType_ShaderResourceView:
                case RootParameterType_UnorderedAccessView:
                {
                    const auto& rootDescriptor = std::get<RootDescriptor>(rootParameter.Child);

                    d3d12RootParameter.Descriptor.ShaderRegister    = rootDescriptor.ShaderRegister;
                    d3d12RootParameter.Descriptor.RegisterSpace     = rootDescriptor.RegisterSpace;

                    break;
                }

                default:
                {
                    return false;
                }
            }

            d3d12RootParameters.push_back(d3d12RootParameter);
        }

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.NumParameters     = d3d12RootParameters.size();
        rootSignatureDesc.pParameters       = d3d12RootParameters.data();
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers   = nullptr;
        rootSignatureDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

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
        GetCommandList()->SetGraphicsRootSignature(m_Handle.Get());
    }

} // namespace Spieler