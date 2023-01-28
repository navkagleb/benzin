#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/root_signature.hpp"

#include "benzin/core/common.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/sampler.hpp"

namespace benzin
{

    namespace
    {

        D3D12_FILTER ConvertToD3D12TextureFilter(const TextureFilter& filter)
        {
            const TextureFilterType minification{ filter.Minification };
            const TextureFilterType magnification{ filter.Magnification };
            const TextureFilterType mipLevel{ filter.MipLevel };

            D3D12_FILTER d3d12Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT };

            if (minification == TextureFilterType::Point)
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
            }
            else if (minification == TextureFilterType::Linear)
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
            }
            else if (minification == TextureFilterType::Anisotropic)
            {
                BENZIN_ASSERT(magnification == TextureFilterType::Anisotropic && mipLevel == TextureFilterType::Anisotropic);

                d3d12Filter = D3D12_FILTER_ANISOTROPIC;
            }

            return d3d12Filter;
        }

        D3D12_ROOT_PARAMETER_TYPE ConvertToD3D12DescriptorType(RootParameter::DescriptorType descriptorType)
        {
            switch (descriptorType)
            {
                case RootParameter::DescriptorType::ConstantBufferView: return D3D12_ROOT_PARAMETER_TYPE_CBV;
                case RootParameter::DescriptorType::ShaderResourceView: return D3D12_ROOT_PARAMETER_TYPE_SRV;
                case RootParameter::DescriptorType::UnorderedAccessView: return D3D12_ROOT_PARAMETER_TYPE_UAV;

                default:
                {
                    BENZIN_WARNING("Unknown DescriptorType!");
                    break;
                }
            }

            return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        }

        D3D12_DESCRIPTOR_RANGE_TYPE ConvertToD3D12RangeType(RootParameter::DescriptorRangeType descriptorRangeType)
        {
            switch (descriptorRangeType)
            {
                case RootParameter::DescriptorRangeType::ShaderResourceView: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                case RootParameter::DescriptorRangeType::UnorderedAccessView: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                case RootParameter::DescriptorRangeType::ConstantBufferView: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;

                default:
                {
                    BENZIN_WARNING("Unknown DescriptorRangeType!");
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
                    .Filter{ ConvertToD3D12TextureFilter(staticSampler.TextureFilter) },
                    .AddressU{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressU) },
                    .AddressV{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressV) },
                    .AddressW{ static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.AddressW) },
                    .MipLODBias{ 0.0f },
                    .MaxAnisotropy{ 1 },
                    .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
                    .BorderColor{ static_cast<D3D12_STATIC_BORDER_COLOR>(staticSampler.BorderColor) },
                    .MinLOD{ 0.0f },
                    .MaxLOD{ D3D12_FLOAT32_MAX },
                    .ShaderRegister{ static_cast<UINT>(staticSampler.ShaderRegister) },
                    .RegisterSpace{ static_cast<UINT>(staticSampler.RegisterSpace) },
                    .ShaderVisibility{ static_cast<D3D12_SHADER_VISIBILITY>(staticSampler.ShaderVisibility) }
                });
            }

            return d3d12StaticSamplers;
        }

    } // anonymous namespace

    //////////////////////////////////////////////////////////////////////////
    /// RootParameter
    //////////////////////////////////////////////////////////////////////////
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
        if (m_D3D12RootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            delete[] m_D3D12RootParameter.DescriptorTable.pDescriptorRanges;
        }
    }

    void RootParameter::InitAs32BitConstants(const _32BitConstants& constants)
    {
        m_D3D12RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        m_D3D12RootParameter.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(constants.ShaderVisibility);

        m_D3D12RootParameter.Constants.Num32BitValues = constants.Count;
        m_D3D12RootParameter.Constants.ShaderRegister = constants.ShaderRegister.Register;
        m_D3D12RootParameter.Constants.RegisterSpace = constants.ShaderRegister.Space;
    }

    void RootParameter::InitAsDescriptor(const Descriptor& descriptor)
    {
        m_D3D12RootParameter.ParameterType = ConvertToD3D12DescriptorType(descriptor.Type);
        m_D3D12RootParameter.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(descriptor.ShaderVisibility);

        m_D3D12RootParameter.Descriptor.ShaderRegister = descriptor.ShaderRegister.Register;
        m_D3D12RootParameter.Descriptor.RegisterSpace = descriptor.ShaderRegister.Space;
    }

    void RootParameter::InitAsDescriptorTable(const DescriptorTable& descriptorTable)
    {
        m_D3D12RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        m_D3D12RootParameter.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(descriptorTable.ShaderVisibility);

        m_D3D12RootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(descriptorTable.Ranges.size());
        m_D3D12RootParameter.DescriptorTable.pDescriptorRanges = new D3D12_DESCRIPTOR_RANGE[descriptorTable.Ranges.size()];

        for (size_t i = 0; i < descriptorTable.Ranges.size(); ++i)
        {
            const DescriptorRange& range = descriptorTable.Ranges[i];
            D3D12_DESCRIPTOR_RANGE& d3d12Range = const_cast<D3D12_DESCRIPTOR_RANGE&>(m_D3D12RootParameter.DescriptorTable.pDescriptorRanges[i]);

            d3d12Range.RangeType = ConvertToD3D12RangeType(range.Type);
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
    RootSignature::RootSignature(Device& device, const Config& config)
    {
        BENZIN_ASSERT(!config.RootParameters.empty());

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
    }

    RootSignature::RootSignature(RootSignature&& other) noexcept
        : m_D3D12RootSignature{ std::exchange(other.m_D3D12RootSignature, nullptr) }
    {}

    RootSignature& RootSignature::operator=(RootSignature&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_D3D12RootSignature = std::exchange(other.m_D3D12RootSignature, nullptr);

        return *this;
    }

} // namespace benzin