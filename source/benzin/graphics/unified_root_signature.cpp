#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>

namespace benzin
{

    static auto CreateUnifiedD3D12RootParameters()
    {
        std::array<D3D12_ROOT_PARAMETER1, magic_enum::enum_count<UnifiedRootParameter>()> d3d12RootParameters = {};

        D3D12_ROOT_PARAMETER1& d3d12RootConsts = d3d12RootParameters[*UnifiedRootParameter::Root32Consts];
        d3d12RootConsts.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        d3d12RootConsts.Constants.ShaderRegister = 0;
        d3d12RootConsts.Constants.RegisterSpace = 0;
        d3d12RootConsts.Constants.Num32BitValues = 32;
        d3d12RootConsts.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER1& d3d12FrameConstBuffer = d3d12RootParameters[*UnifiedRootParameter::FrameConsts];
        d3d12FrameConstBuffer.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        d3d12FrameConstBuffer.Descriptor.ShaderRegister = 0;
        d3d12FrameConstBuffer.Descriptor.RegisterSpace = 1;
        d3d12FrameConstBuffer.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER1& d3d12RenderPassConstBuffer0 = d3d12RootParameters[*UnifiedRootParameter::RenderPassConsts];
        d3d12RenderPassConstBuffer0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        d3d12RenderPassConstBuffer0.Descriptor.ShaderRegister = 0;
        d3d12RenderPassConstBuffer0.Descriptor.RegisterSpace = 2;
        d3d12RenderPassConstBuffer0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER1& d3d12GpuPrintConstBuffer = d3d12RootParameters[*UnifiedRootParameter::GpuPrintConsts];
        d3d12GpuPrintConstBuffer.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        d3d12GpuPrintConstBuffer.Descriptor.ShaderRegister = 0;
        d3d12GpuPrintConstBuffer.Descriptor.RegisterSpace = 3;
        d3d12GpuPrintConstBuffer.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER1& d3d12LightBuffer = d3d12RootParameters[*UnifiedRootParameter::SunLightConsts];
        d3d12LightBuffer.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        d3d12LightBuffer.Descriptor.ShaderRegister = 0;
        d3d12LightBuffer.Descriptor.RegisterSpace = 4;
        d3d12LightBuffer.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER1& d3d12Tlas = d3d12RootParameters[*UnifiedRootParameter::SceneTlas];
        d3d12Tlas.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        d3d12Tlas.Descriptor.ShaderRegister = 0;
        d3d12Tlas.Descriptor.RegisterSpace = 0;
        d3d12Tlas.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER1& d3d12ReadbackStats = d3d12RootParameters[*UnifiedRootParameter::ReadbackStatsBuffer];
        d3d12ReadbackStats.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        d3d12ReadbackStats.Descriptor.ShaderRegister = 0;
        d3d12ReadbackStats.Descriptor.RegisterSpace = 0;
        d3d12ReadbackStats.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        return d3d12RootParameters;
    }

    static D3D12_STATIC_SAMPLER_DESC CreateD3D12StaticSamplerDesc(
        D3D12_FILTER d3d12Filter,
        D3D12_TEXTURE_ADDRESS_MODE d3d12AddressMode,
        uint32_t spaceIndex)
    {
        D3D12_STATIC_SAMPLER_DESC d3d12Sampler = {};
        d3d12Sampler.Filter = d3d12Filter;
        d3d12Sampler.AddressU = d3d12AddressMode;
        d3d12Sampler.AddressV = d3d12AddressMode;
        d3d12Sampler.AddressW = d3d12AddressMode;
        d3d12Sampler.MipLODBias = 0.0f;
        d3d12Sampler.MaxAnisotropy = 1;
        d3d12Sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        d3d12Sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        d3d12Sampler.MinLOD = 0.0f;
        d3d12Sampler.MaxLOD = D3D12_FLOAT32_MAX;
        d3d12Sampler.ShaderRegister = 0;
        d3d12Sampler.RegisterSpace = spaceIndex;
        d3d12Sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        return d3d12Sampler;
    }

    //

    UnifiedRootSignature::UnifiedRootSignature(Device& device)
    {
        const auto d3d12RootParameters = CreateUnifiedD3D12RootParameters();

        const auto d3d12StaticSamplerDescs = std::to_array(
        {
            CreateD3D12StaticSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0),
            CreateD3D12StaticSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 1),
            CreateD3D12StaticSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 2),
            CreateD3D12StaticSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 3),
            CreateD3D12StaticSamplerDesc(D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 4),
        });

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12RootSignatureDesc = {};
        d3d12RootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        d3d12RootSignatureDesc.Desc_1_1.NumParameters = (uint32_t)d3d12RootParameters.size();
        d3d12RootSignatureDesc.Desc_1_1.pParameters = d3d12RootParameters.data(),
        d3d12RootSignatureDesc.Desc_1_1.NumStaticSamplers = (uint32_t)d3d12StaticSamplerDescs.size(),
        d3d12RootSignatureDesc.Desc_1_1.pStaticSamplers = d3d12StaticSamplerDescs.data(),
        d3d12RootSignatureDesc.Desc_1_1.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

        ComPtr<ID3DBlob> d3d12Blob;
        ComPtr<ID3DBlob> d3d12Error;
        const HRESULT hr = D3D12SerializeVersionedRootSignature(&d3d12RootSignatureDesc, &d3d12Blob, &d3d12Error);
        if (FAILED(hr))
        {
            BenzinD3D12Call(hr, "Failed to Serialize RootSignature. Error: {}", (const char*)d3d12Error->GetBufferPointer());
        }

        BenzinD3D12Call(device.GetD3D12Device()->CreateRootSignature(
            0,
            d3d12Blob->GetBufferPointer(),
            d3d12Blob->GetBufferSize(),
            IID_PPV_ARGS(&m_D3D12RootSignature)));

        SetD3DObjectDebugName(m_D3D12RootSignature, "UnifiedRootSignature");
    }

    UnifiedRootSignature::~UnifiedRootSignature()
    {
        SafeReleaseD3DObject(m_D3D12RootSignature);
    }

}
