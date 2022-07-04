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

        static D3D12_ROOT_PARAMETER_TYPE ConvertToD3D12DescriptorType(RootParameter::DescriptorType descriptorType)
        {
            switch (descriptorType)
            {
                case RootParameter::DescriptorType::ConstantBufferView: return D3D12_ROOT_PARAMETER_TYPE_CBV;
                case RootParameter::DescriptorType::ShaderResourceView: return D3D12_ROOT_PARAMETER_TYPE_SRV;
                case RootParameter::DescriptorType::UnorderedAccessView: return D3D12_ROOT_PARAMETER_TYPE_UAV;

                default:
                {
                    SPIELER_WARNING("Unknown DescriptorType!");
                    break;
                }
            }

            return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        }

        static D3D12_DESCRIPTOR_RANGE_TYPE ConvertToD3D12RangeType(RootParameter::DescriptorRangeType descriptorRangeType)
        {
            switch (descriptorRangeType)
            {
                case RootParameter::DescriptorRangeType::ShaderResourceView: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                case RootParameter::DescriptorRangeType::UnorderedAccessView: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                case RootParameter::DescriptorRangeType::ConstantBufferView: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

                default:
                {
                    SPIELER_WARNING("Unknown DescriptorRangeType!");
                    break;
                }
            }

            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        }

        static std::vector<D3D12_STATIC_SAMPLER_DESC> ToD3D12StaticSamplers(const std::vector<StaticSampler>& staticSamplers)
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

    // RootParameter
    RootParameter::RootParameter(const _32BitConstants& constants)
    {
        InitAs32BitConstants(constants);
    }

    RootParameter::RootParameter(const Descriptor& descriptor)
    {
        InitAsDescriptor(descriptor);
    }

    RootParameter::RootParameter(const DescriptorTable& descriptorTable)
    {
        InitAsDescriptorTable(descriptorTable);
    }

    RootParameter::RootParameter(const SingleDescriptorTable& singleDescriptorTable)
    {
        InitAsSingleDescriptorTable(singleDescriptorTable);
    }

    RootParameter::RootParameter(RootParameter&& other) noexcept
    {
        Swap(std::move(other));
    }           

    RootParameter::~RootParameter()
    {
        if (m_RootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            delete[] m_RootParameter.DescriptorTable.pDescriptorRanges;
        }
    }

    void RootParameter::InitAs32BitConstants(const _32BitConstants& constants)
    {
        m_RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        m_RootParameter.ShaderVisibility = D3D12Converter::Convert(constants.ShaderVisibility);

        m_RootParameter.Constants.Num32BitValues = constants.Count;
        m_RootParameter.Constants.ShaderRegister = constants.ShaderRegister.Register;
        m_RootParameter.Constants.RegisterSpace = constants.ShaderRegister.Space;
    }

    void RootParameter::InitAsDescriptor(const Descriptor& descriptor)
    {
        m_RootParameter.ParameterType = _internal::ConvertToD3D12DescriptorType(descriptor.Type);
        m_RootParameter.ShaderVisibility = D3D12Converter::Convert(descriptor.ShaderVisibility);

        m_RootParameter.Descriptor.ShaderRegister = descriptor.ShaderRegister.Register;
        m_RootParameter.Descriptor.RegisterSpace = descriptor.ShaderRegister.Space;
    }

    void RootParameter::InitAsDescriptorTable(const DescriptorTable& descriptorTable)
    {
        m_RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        m_RootParameter.ShaderVisibility = D3D12Converter::Convert(descriptorTable.ShaderVisibility);

        m_RootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(descriptorTable.Ranges.size());
        m_RootParameter.DescriptorTable.pDescriptorRanges = new D3D12_DESCRIPTOR_RANGE[descriptorTable.Ranges.size()];

        for (size_t i = 0; i < descriptorTable.Ranges.size(); ++i)
        {
            const DescriptorRange& range{ descriptorTable.Ranges[i] };
            D3D12_DESCRIPTOR_RANGE& d3d12Range{ const_cast<D3D12_DESCRIPTOR_RANGE&>(m_RootParameter.DescriptorTable.pDescriptorRanges[i]) };

            d3d12Range.RangeType = _internal::ConvertToD3D12RangeType(range.Type);
            d3d12Range.NumDescriptors = range.DescriptorCount;
            d3d12Range.BaseShaderRegister = range.BaseShaderRegister.Register;
            d3d12Range.RegisterSpace = range.BaseShaderRegister.Space;
            d3d12Range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        }
    }

    void RootParameter::InitAsSingleDescriptorTable(const SingleDescriptorTable& singleDescriptorTable)
    {
        InitAsDescriptorTable(DescriptorTable
        {
            .ShaderVisibility{ singleDescriptorTable.ShaderVisibility },
            .Ranges{ singleDescriptorTable.Range }
        });
    }

    void RootParameter::Swap(RootParameter&& other) noexcept
    {
        m_RootParameter.ParameterType = other.m_RootParameter.ParameterType;
        m_RootParameter.ShaderVisibility = other.m_RootParameter.ShaderVisibility;

        switch (m_RootParameter.ParameterType)
        {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            {
                m_RootParameter.DescriptorTable.NumDescriptorRanges = std::exchange(other.m_RootParameter.DescriptorTable.NumDescriptorRanges, 0);
                m_RootParameter.DescriptorTable.pDescriptorRanges = std::exchange(other.m_RootParameter.DescriptorTable.pDescriptorRanges, nullptr);

                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            {
                m_RootParameter.Constants = other.m_RootParameter.Constants;

                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
            {
                m_RootParameter.Descriptor = other.m_RootParameter.Descriptor;

                break;
            }
        }
    }

    RootParameter& RootParameter::operator=(RootParameter&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Swap(std::move(other));

        return *this;
    }

    // RootSignature
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

        const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc
        {
            .NumParameters{ static_cast<UINT>(rootParameters.size()) },
            .pParameters{ reinterpret_cast<const D3D12_ROOT_PARAMETER*>(rootParameters.data()) },
            .NumStaticSamplers{ 0 },
            .pStaticSamplers{ nullptr },
            .Flags{ D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT }
        };

        SPIELER_RETURN_IF_FAILED(Init(device, rootSignatureDesc));

        return true;
    }

    bool RootSignature::Init(Device& device, const std::vector<RootParameter>& rootParameters, const std::vector<StaticSampler>& staticSamplers)
    {
        SPIELER_ASSERT(!rootParameters.empty());
        SPIELER_ASSERT(!staticSamplers.empty());

        const std::vector<D3D12_STATIC_SAMPLER_DESC> d3d12StaticSamplers{ _internal::ToD3D12StaticSamplers(staticSamplers) };

        const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc
        {
            .NumParameters{ static_cast<UINT>(rootParameters.size()) },
            .pParameters{ reinterpret_cast<const D3D12_ROOT_PARAMETER*>(rootParameters.data()) },
            .NumStaticSamplers{ static_cast<UINT>(d3d12StaticSamplers.size()) },
            .pStaticSamplers{ d3d12StaticSamplers.data() },
            .Flags{ D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT }
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