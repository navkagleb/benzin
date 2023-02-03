#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/root_signature.hpp"

#include "benzin/core/common.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/sampler.hpp"

namespace benzin
{

    namespace
    {

        uint32_t g_RootSignatureCounter = 0;

        D3D12_FILTER ConvertToD3D12TextureFilter(const Sampler::TextureFilterType& minification, const Sampler::TextureFilterType& magnification, const Sampler::TextureFilterType& mipLevel)
        {
            using TextureFilterType = Sampler::TextureFilterType;

            D3D12_FILTER d3d12Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

            switch (minification)
            {
                case TextureFilterType::Point:
                {
                    BENZIN_ASSERT(magnification != TextureFilterType::Anisotropic && mipLevel != TextureFilterType::Anisotropic);

                    if (magnification == TextureFilterType::Point)
                    {
                        d3d12Filter = mipLevel == TextureFilterType::Point ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                    }
                    else
                    {
                        d3d12Filter = mipLevel == TextureFilterType::Point ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
                    }

                    break;
                }
                case TextureFilterType::Linear:
                {
                    BENZIN_ASSERT(magnification != TextureFilterType::Anisotropic && mipLevel != TextureFilterType::Anisotropic);

                    if (magnification == TextureFilterType::Point)
                    {
                        d3d12Filter = mipLevel == TextureFilterType::Point ? D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT : D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
                    }
                    else
                    {
                        d3d12Filter = mipLevel == TextureFilterType::Point ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                    }

                    break;
                }
                case TextureFilterType::Anisotropic:
                {
                    BENZIN_ASSERT(magnification == TextureFilterType::Anisotropic && mipLevel == TextureFilterType::Anisotropic);

                    d3d12Filter = D3D12_FILTER_ANISOTROPIC;

                    break;
                }
                default:
                {
                    BENZIN_CRITICAL("Unknown TextureFilterType");
                    BENZIN_ASSERT(false);

                    break;
                }
            }

            return d3d12Filter;
        }

        D3D12_ROOT_PARAMETER_TYPE ConvertToD3D12DescriptorType(Descriptor::Type descriptorType)
        {
            switch (descriptorType)
            {
                case Descriptor::Type::ConstantBufferView: return D3D12_ROOT_PARAMETER_TYPE_CBV;
                case Descriptor::Type::ShaderResourceView: return D3D12_ROOT_PARAMETER_TYPE_SRV;
                case Descriptor::Type::UnorderedAccessView: return D3D12_ROOT_PARAMETER_TYPE_UAV;

                default:
                {
                    BENZIN_WARNING("Unknown DescriptorType for RootParameter!");
                    break;
                }
            }

            return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        }

        D3D12_DESCRIPTOR_RANGE_TYPE ConvertToD3D12DescriptorRangeType(Descriptor::Type descriptorType)
        {
            switch (descriptorType)
            {
            case Descriptor::Type::ConstantBufferView: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            case Descriptor::Type::ShaderResourceView: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            case Descriptor::Type::UnorderedAccessView: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            case Descriptor::Type::Sampler: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

            default:
            {
                BENZIN_WARNING("Unknown DescriptorType for RootParameter!");
                break;
            }
            }

            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        }

        std::vector<D3D12_STATIC_SAMPLER_DESC> ConvertToD3D12StaticSamplers(const std::vector<StaticSampler>& staticSamplers)
        {
            if (staticSamplers.empty())
            {
                return {};
            }

            std::vector<D3D12_STATIC_SAMPLER_DESC> d3d12StaticSamplers;
            d3d12StaticSamplers.reserve(staticSamplers.size());

            for (const StaticSampler& staticSampler : staticSamplers)
            {
                d3d12StaticSamplers.push_back(D3D12_STATIC_SAMPLER_DESC
                {
                    .Filter{ ConvertToD3D12TextureFilter(staticSampler.Sampler.Minification, staticSampler.Sampler.Magnification, staticSampler.Sampler.MipLevel) },
                    .AddressU{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.Sampler.AddressU) },
                    .AddressV{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.Sampler.AddressV) },
                    .AddressW{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.Sampler.AddressW) },
                    .MipLODBias{ staticSampler.MipLODBias },
                    .MaxAnisotropy{ staticSampler.MaxAnisotropy },
                    .ComparisonFunc{ static_cast<D3D12_COMPARISON_FUNC>(staticSampler.ComparisonFunction) },
                    .BorderColor{ static_cast<D3D12_STATIC_BORDER_COLOR>(staticSampler.BorderColor) },
                    .MinLOD{ 0.0f },
                    .MaxLOD{ D3D12_FLOAT32_MAX },
                    .ShaderRegister{ static_cast<UINT>(staticSampler.ShaderRegister.Index) },
                    .RegisterSpace{ static_cast<UINT>(staticSampler.ShaderRegister.Space) },
                    .ShaderVisibility{ static_cast<D3D12_SHADER_VISIBILITY>(staticSampler.ShaderVisibility) }
                });
            }

            return d3d12StaticSamplers;
        }

    } // anonymous namespace

    //////////////////////////////////////////////////////////////////////////
    /// RootParameter
    //////////////////////////////////////////////////////////////////////////
    RootParameter::RootParameter(const Constants32BitConfig& constants32BitConfig)
    {
        InitAsConstants32Bit(constants32BitConfig);
    }

    RootParameter::RootParameter(const DescriptorConfig& descriptorConfig)
    {
        InitAsDescriptor(descriptorConfig);
    }

    RootParameter::RootParameter(const DescriptorTableConfig& descriptorTableConfig)
    {
        InitAsDescriptorTable(descriptorTableConfig);
    }

    RootParameter::RootParameter(const SingleDescriptorTableConfig& singleDescriptorTableConfig)
    {
        InitAsSingleDescriptorTable(singleDescriptorTableConfig);
    }

    RootParameter::RootParameter(RootParameter&& other) noexcept
    {
        Swap(std::move(other));
    }           

    RootParameter::~RootParameter()
    {
        if (m_D3D12RootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            delete[] m_D3D12RootParameter.DescriptorTable.pDescriptorRanges;
        }
    }

    void RootParameter::InitAsConstants32Bit(const Constants32BitConfig& constants32BitConfig)
    {
        m_D3D12RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        m_D3D12RootParameter.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(constants32BitConfig.ShaderVisibility);

        m_D3D12RootParameter.Constants.Num32BitValues = constants32BitConfig.Count;

        m_D3D12RootParameter.Constants.ShaderRegister = constants32BitConfig.ShaderRegister.Index;
        m_D3D12RootParameter.Constants.RegisterSpace = constants32BitConfig.ShaderRegister.Space;
    }

    void RootParameter::InitAsDescriptor(const DescriptorConfig& descriptorConfig)
    {
        m_D3D12RootParameter.ParameterType = ConvertToD3D12DescriptorType(descriptorConfig.Type);
        m_D3D12RootParameter.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(descriptorConfig.ShaderVisibility);

        m_D3D12RootParameter.Descriptor.ShaderRegister = descriptorConfig.ShaderRegister.Index;
        m_D3D12RootParameter.Descriptor.RegisterSpace = descriptorConfig.ShaderRegister.Space;
    }

    void RootParameter::InitAsDescriptorTable(const DescriptorTableConfig& descriptorTableConfig)
    {
        m_D3D12RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        m_D3D12RootParameter.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(descriptorTableConfig.ShaderVisibility);

        m_D3D12RootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(descriptorTableConfig.Ranges.size());
        m_D3D12RootParameter.DescriptorTable.pDescriptorRanges = new D3D12_DESCRIPTOR_RANGE[descriptorTableConfig.Ranges.size()];

        for (size_t i = 0; i < descriptorTableConfig.Ranges.size(); ++i)
        {
            const DescriptorRange& range = descriptorTableConfig.Ranges[i];
            D3D12_DESCRIPTOR_RANGE& d3d12Range = const_cast<D3D12_DESCRIPTOR_RANGE&>(m_D3D12RootParameter.DescriptorTable.pDescriptorRanges[i]);

            d3d12Range.RangeType = ConvertToD3D12DescriptorRangeType(range.Type);
            d3d12Range.NumDescriptors = range.DescriptorCount;
            d3d12Range.BaseShaderRegister = range.BaseShaderRegister.Index;
            d3d12Range.RegisterSpace = range.BaseShaderRegister.Space;
            d3d12Range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        }
    }

    void RootParameter::InitAsSingleDescriptorTable(const SingleDescriptorTableConfig& singleDescriptorTableConfig)
    {
        InitAsDescriptorTable(DescriptorTableConfig
        {
            .ShaderVisibility{ singleDescriptorTableConfig.ShaderVisibility },
            .Ranges{ singleDescriptorTableConfig.Range }
        });
    }

    void RootParameter::Swap(RootParameter&& other) noexcept
    {
        m_D3D12RootParameter.ParameterType = other.m_D3D12RootParameter.ParameterType;
        m_D3D12RootParameter.ShaderVisibility = other.m_D3D12RootParameter.ShaderVisibility;

        switch (m_D3D12RootParameter.ParameterType)
        {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            {
                m_D3D12RootParameter.DescriptorTable.NumDescriptorRanges = std::exchange(other.m_D3D12RootParameter.DescriptorTable.NumDescriptorRanges, 0);
                m_D3D12RootParameter.DescriptorTable.pDescriptorRanges = std::exchange(other.m_D3D12RootParameter.DescriptorTable.pDescriptorRanges, nullptr);

                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            {
                m_D3D12RootParameter.Constants = other.m_D3D12RootParameter.Constants;

                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
            {
                m_D3D12RootParameter.Descriptor = other.m_D3D12RootParameter.Descriptor;

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

    //////////////////////////////////////////////////////////////////////////
    /// RootSignature
    //////////////////////////////////////////////////////////////////////////
    RootSignature::RootSignature(Device& device, const Config& config, const char* debugName)
    {
        BENZIN_ASSERT(!config.RootParameters.empty());
        BENZIN_ASSERT(!m_D3D12RootSignature);

        const std::vector<D3D12_STATIC_SAMPLER_DESC> d3d12StaticSamplers = ConvertToD3D12StaticSamplers(config.StaticSamplers);

        const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc
        {
            .NumParameters{ static_cast<UINT>(config.RootParameters.size()) },
            .pParameters{ reinterpret_cast<const D3D12_ROOT_PARAMETER*>(config.RootParameters.data()) },
            .NumStaticSamplers{ static_cast<UINT>(d3d12StaticSamplers.size()) },
            .pStaticSamplers{ d3d12StaticSamplers.data() },
            .Flags{ D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT }
        };

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
            BENZIN_CRITICAL("{}", reinterpret_cast<const char*>(error->GetBufferPointer()));
            return;
        }

        BENZIN_D3D12_ASSERT(result);

        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateRootSignature(
            0,
            serializedRootSignature->GetBufferPointer(),
            serializedRootSignature->GetBufferSize(),
            IID_PPV_ARGS(&m_D3D12RootSignature)
        ));

        SetDebugName(!debugName ? std::to_string(g_RootSignatureCounter) : std::string{ debugName });

        BENZIN_INFO("{} created", GetDebugName());

        g_RootSignatureCounter++;
    }

    RootSignature::~RootSignature()
    {
        BENZIN_INFO("{} destroyed", GetDebugName());

        SafeReleaseD3D12Object(m_D3D12RootSignature);
    }

} // namespace benzin
