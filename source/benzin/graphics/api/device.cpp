#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/device.hpp"

#include "benzin/graphics/api/utils.hpp"
#include "benzin/graphics/api/resource_view_builder.hpp"
#include "benzin/graphics/api/sampler.hpp"
#include "benzin/graphics/resource_loader.hpp"

namespace benzin
{
    namespace
    {

        void EnableD3D12DebugLayer()
        {
#if defined(BENZIN_DEBUG)
            ComPtr<ID3D12Debug> d3d12Debug;
            BENZIN_D3D12_ASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));
            d3d12Debug->EnableDebugLayer();

            ComPtr<ID3D12Debug3> d3d12Debug3;
            BENZIN_D3D12_ASSERT(d3d12Debug.As(&d3d12Debug3));
            d3d12Debug3->SetEnableGPUBasedValidation(false); // TODO: replace with true
            d3d12Debug3->SetEnableSynchronizedCommandQueueValidation(true);
#endif // defined(BENZIN_DEBUG)
        }

        void BreakOnD3D12Error(ID3D12Device* d3d12Device, bool isBreak)
        {
#if defined(BENZIN_DEBUG)
            ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
            BENZIN_D3D12_ASSERT(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));

            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, isBreak);
            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, isBreak);
            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, isBreak);
#endif // defined(BENZIN_DEBUG)
        }

        void ReportLiveD3D12Objects(ID3D12Device* d3d12Device)
        {
#if defined(BENZIN_DEBUG)
            using namespace magic_enum::bitwise_operators;

            ComPtr<ID3D12DebugDevice> d3d12DebugDevice;
            BENZIN_D3D12_ASSERT(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));

            d3d12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL);
#endif // defined(BENZIN_DEBUG)
        }

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

        D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12Type)
        {
            return D3D12_HEAP_PROPERTIES
            {
                .Type{ d3d12Type },
                .CPUPageProperty{ D3D12_CPU_PAGE_PROPERTY_UNKNOWN },
                .MemoryPoolPreference{ D3D12_MEMORY_POOL_UNKNOWN },
                .CreationNodeMask{ 1 },
                .VisibleNodeMask{ 1 },
            };
        }

        D3D12_RESOURCE_DESC ConvertToD3D12ResourceDesc(const BufferResource::Config& config)
        {
            return D3D12_RESOURCE_DESC
            {
                .Dimension{ D3D12_RESOURCE_DIMENSION_BUFFER },
                .Alignment{ 0 },
                .Width{ static_cast<uint64_t>(config.ElementSize) * config.ElementCount },
                .Height{ 1 },
                .DepthOrArraySize{ 1 },
                .MipLevels{ 1 },
                .Format{ DXGI_FORMAT_UNKNOWN },
                .SampleDesc{ 1, 0 },
                .Layout{ D3D12_TEXTURE_LAYOUT_ROW_MAJOR },
                .Flags{ D3D12_RESOURCE_FLAG_NONE }
            };
        }

        BufferResource::Config ValidateBufferResourceConfig(const BufferResource::Config& config)
        {
            using namespace magic_enum::bitwise_operators;

            BufferResource::Config validatedConfig = config;

            if ((validatedConfig.Flags & BufferResource::Flags::ConstantBuffer) != BufferResource::Flags::None)
            {
                //validatedConfig.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT; // TODO
                validatedConfig.ElementSize = Align<uint32_t>(validatedConfig.ElementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
            }

            return validatedConfig;
        }

        D3D12_RESOURCE_DESC ConvertToD3D12ResourceDesc(const TextureResource::Config& config)
        {
            return D3D12_RESOURCE_DESC
            {
                .Dimension{ static_cast<D3D12_RESOURCE_DIMENSION>(config.Type) },
                .Alignment{ 0 },
                .Width{ static_cast<uint64_t>(config.Width) },
                .Height{ config.Height },
                .DepthOrArraySize{ config.ArraySize },
                .MipLevels{ config.MipCount },
                .Format{ static_cast<DXGI_FORMAT>(config.Format) },
                .SampleDesc{ 1, 0 },
                .Layout{ D3D12_TEXTURE_LAYOUT_UNKNOWN },
                .Flags{ static_cast<D3D12_RESOURCE_FLAGS>(config.Flags) }
            };
        }

        D3D12_CLEAR_VALUE ConvertToD3D12ClearValue(const TextureResource::Config& config, const DirectX::XMFLOAT4& clearColor)
        {
            return D3D12_CLEAR_VALUE
            {
                .Format{ static_cast<DXGI_FORMAT>(config.Format) },
                .Color 
                { 
                    clearColor.x,
                    clearColor.y,
                    clearColor.z,
                    clearColor.w
                }
            };
        }

        D3D12_CLEAR_VALUE ConvertToD3D12ClearValue(const TextureResource::Config& config, float clearDepth, uint8_t clearStencil)
        {
            return D3D12_CLEAR_VALUE
            {
                .Format{ static_cast<DXGI_FORMAT>(config.Format) },
                .DepthStencil
                {
                    .Depth{ clearDepth },
                    .Stencil{ clearStencil }
                }
            };
        }

        TextureResource::Config ConvertFromD3D12ResourceDesc(const D3D12_RESOURCE_DESC& d3d12ResourceDesc)
        {
            return TextureResource::Config
            {
                .Type{ static_cast<TextureResource::Type>(d3d12ResourceDesc.Dimension) },
                .Width{ static_cast<uint32_t>(d3d12ResourceDesc.Width) },
                .Height{ d3d12ResourceDesc.Height },
                .ArraySize{ d3d12ResourceDesc.DepthOrArraySize },
                .MipCount{ d3d12ResourceDesc.MipLevels },
                .Format{ static_cast<GraphicsFormat>(d3d12ResourceDesc.Format) },
                .Flags{ static_cast<TextureResource::Flags>(d3d12ResourceDesc.Flags) }
            };
        }

    } // anonymous namespace

    Device::Device(std::string_view debugName)
    {
        EnableD3D12DebugLayer();

        BENZIN_D3D12_ASSERT(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_D3D12Device)));
        SetDebugName(debugName, true);

        BreakOnD3D12Error(m_D3D12Device, true);

        CreateBindlessRootSignature();

        m_DescriptorManager = new DescriptorManager{ *this };
        m_ResourceLoader = new ResourceLoader{ *this };
        m_ResourceViewBuilder = new ResourceViewBuilder{ *this };

        m_ResourceReleaser = [&](Resource* resource)
        {
            BENZIN_ASSERT(m_DescriptorManager);

            resource->ReleaseViews(*m_DescriptorManager);

            delete resource;
        };
    }

    Device::~Device()
    {
        delete m_ResourceViewBuilder;
        delete m_ResourceLoader;
        delete m_DescriptorManager;

        BreakOnD3D12Error(m_D3D12Device, false);

        SafeReleaseD3D12Object(m_D3D12BindlessRootSignature);

        ReportLiveD3D12Objects(m_D3D12Device);
        SafeReleaseD3D12Object(m_D3D12Device);
    }

    std::shared_ptr<BufferResource> Device::CreateBufferResource(const BufferResource::Config& config, std::string_view debugName) const
    {
        BENZIN_ASSERT(config.ElementSize != 0);
        BENZIN_ASSERT(config.ElementCount != 0);

        const BufferResource::Config validatedConfig = ValidateBufferResourceConfig(config);

        const D3D12_HEAP_TYPE d3d12HeapType = config.Flags == BufferResource::Flags::None ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ConvertToD3D12ResourceDesc(validatedConfig);

        auto* rawBufferResource = new BufferResource
        {
            CreateD3D12CommittedResource(d3d12HeapType, &d3d12ResourceDesc, nullptr),
            ValidateBufferResourceConfig(validatedConfig),
            debugName
        };

        return std::shared_ptr<BufferResource>{ rawBufferResource, m_ResourceReleaser };
    }

    std::shared_ptr<TextureResource> Device::RegisterTextureResource(ID3D12Resource* d3d12Resource, std::string_view debugName) const
    {
        BENZIN_ASSERT(d3d12Resource);

        auto* rawTextureResource = new TextureResource
        {
            d3d12Resource,
            ConvertFromD3D12ResourceDesc(d3d12Resource->GetDesc()),
            debugName
        };

        return std::shared_ptr<TextureResource>{ rawTextureResource, m_ResourceReleaser };
    }

    std::shared_ptr<TextureResource> Device::CreateTextureResource(const TextureResource::Config& config, std::string_view debugName) const
    {
        using namespace magic_enum::bitwise_operators;
        using Flags = TextureResource::Flags;

        const D3D12_HEAP_TYPE d3d12HeapType = D3D12_HEAP_TYPE_DEFAULT;
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ConvertToD3D12ResourceDesc(config);
        
        ID3D12Resource* d3d12Resource = nullptr;

        if ((config.Flags & Flags::BindAsRenderTarget) != Flags::None)
        {
            const D3D12_CLEAR_VALUE d3d12ClearValue = ConvertToD3D12ClearValue(config, TextureResource::GetDefaultClearColor());

            d3d12Resource = CreateD3D12CommittedResource(d3d12HeapType, &d3d12ResourceDesc, &d3d12ClearValue);
        }
        else if ((config.Flags & Flags::BindAsDepthStencil) != Flags::None)
        {
            const D3D12_CLEAR_VALUE d3d12ClearValue = ConvertToD3D12ClearValue(config, TextureResource::GetDefaultClearDepth(), TextureResource::GetDefaultClearStencil());

            d3d12Resource = CreateD3D12CommittedResource(d3d12HeapType, &d3d12ResourceDesc, &d3d12ClearValue);
        }
        else
        {
            d3d12Resource = CreateD3D12CommittedResource(d3d12HeapType, &d3d12ResourceDesc, nullptr);
        }

        BENZIN_ASSERT(d3d12Resource);

        auto* rawTextureResource = new TextureResource{ d3d12Resource, config, debugName };

        return std::shared_ptr<TextureResource>{ rawTextureResource, m_ResourceReleaser };
    }

    void Device::CreateBindlessRootSignature()
    {
        // Check for support
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12OptionalFeatures{};
            BENZIN_D3D12_ASSERT(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12OptionalFeatures, sizeof(d3d12OptionalFeatures)));
            BENZIN_ASSERT(d3d12OptionalFeatures.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_3);

            D3D12_FEATURE_DATA_SHADER_MODEL d3d12FeatureDataShaderModel
            {
                .HighestShaderModel{ D3D_SHADER_MODEL_6_6 }
            };

            BENZIN_D3D12_ASSERT(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &d3d12FeatureDataShaderModel, sizeof(d3d12FeatureDataShaderModel)));
            BENZIN_ASSERT(d3d12FeatureDataShaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6);
        }

        const D3D12_ROOT_PARAMETER1 d3d12RootParameter
        {
            .ParameterType{ D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS },
            .Constants
            {
                .ShaderRegister{ 0 },
                .RegisterSpace{ 0 },
                .Num32BitValues{ 32 }
            },
            .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
        };

        const std::array<D3D12_STATIC_SAMPLER_DESC, 7> d3d12StaticSamplers
        {
            D3D12_STATIC_SAMPLER_DESC
            {
                .Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT },
                .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .MipLODBias{ 0.0f },
                .MaxAnisotropy{ 1 },
                .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
                .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
                .MinLOD{ 0.0f },
                .MaxLOD{ D3D12_FLOAT32_MAX },
                .ShaderRegister{ 0 },
                .RegisterSpace{ 0 },
                .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
            },
            D3D12_STATIC_SAMPLER_DESC
            {
                .Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT },
                .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .MipLODBias{ 0.0f },
                .MaxAnisotropy{ 1 },
                .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
                .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
                .MinLOD{ 0.0f },
                .MaxLOD{ D3D12_FLOAT32_MAX },
                .ShaderRegister{ 1 },
                .RegisterSpace{ 0 },
                .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
            },
            D3D12_STATIC_SAMPLER_DESC
            {
                .Filter{ D3D12_FILTER_MIN_MAG_MIP_LINEAR },
                .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .MipLODBias{ 0.0f },
                .MaxAnisotropy{ 1 },
                .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
                .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
                .MinLOD{ 0.0f },
                .MaxLOD{ D3D12_FLOAT32_MAX },
                .ShaderRegister{ 2 },
                .RegisterSpace{ 0 },
                .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
            },
            D3D12_STATIC_SAMPLER_DESC
            {
                .Filter{ D3D12_FILTER_MIN_MAG_MIP_LINEAR },
                .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .MipLODBias{ 0.0f },
                .MaxAnisotropy{ 1 },
                .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
                .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
                .MinLOD{ 0.0f },
                .MaxLOD{ D3D12_FLOAT32_MAX },
                .ShaderRegister{ 3 },
                .RegisterSpace{ 0 },
                .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
            },
            D3D12_STATIC_SAMPLER_DESC
            {
                .Filter{ D3D12_FILTER_ANISOTROPIC },
                .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_WRAP },
                .MipLODBias{ 0.0f },
                .MaxAnisotropy{ 1 },
                .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
                .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
                .MinLOD{ 0.0f },
                .MaxLOD{ D3D12_FLOAT32_MAX },
                .ShaderRegister{ 4 },
                .RegisterSpace{ 0 },
                .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
            },
            D3D12_STATIC_SAMPLER_DESC
            {
                .Filter{ D3D12_FILTER_ANISOTROPIC },
                .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_CLAMP },
                .MipLODBias{ 0.0f },
                .MaxAnisotropy{ 1 },
                .ComparisonFunc{ D3D12_COMPARISON_FUNC_ALWAYS },
                .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
                .MinLOD{ 0.0f },
                .MaxLOD{ D3D12_FLOAT32_MAX },
                .ShaderRegister{ 5 },
                .RegisterSpace{ 0 },
                .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
            },
            D3D12_STATIC_SAMPLER_DESC
            {
                .Filter{ D3D12_FILTER_MIN_MAG_MIP_LINEAR },
                .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
                .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
                .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
                .MipLODBias{ 0.0f },
                .MaxAnisotropy{ 16 },
                .ComparisonFunc{ D3D12_COMPARISON_FUNC_LESS_EQUAL },
                .BorderColor{ D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK },
                .MinLOD{ 0.0f },
                .MaxLOD{ D3D12_FLOAT32_MAX },
                .ShaderRegister{ 6 },
                .RegisterSpace{ 0 },
                .ShaderVisibility{ D3D12_SHADER_VISIBILITY_ALL }
            },
        };

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12VersionedRootSignatureDesc
        {
            .Version{ D3D_ROOT_SIGNATURE_VERSION_1_1 },
            .Desc_1_1
            {
                .NumParameters{ 1 },
                .pParameters{ &d3d12RootParameter },
                .NumStaticSamplers{ static_cast<UINT>(d3d12StaticSamplers.size()) },
                .pStaticSamplers{ d3d12StaticSamplers.data() },
                .Flags
                { 
                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | 
                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
                }
            }
        };

        ComPtr<ID3DBlob> d3d12SerializedRootSignatureBlob;
        ComPtr<ID3DBlob> d3d12ErrorBlob;

        const HRESULT result = D3D12SerializeVersionedRootSignature(
            &d3d12VersionedRootSignatureDesc,
            &d3d12SerializedRootSignatureBlob,
            &d3d12ErrorBlob
        );

        if (FAILED(result))
        {
            if (d3d12ErrorBlob && d3d12ErrorBlob->GetBufferSize() != 0)
            {
                BENZIN_CRITICAL(
                    "Failed to create BindlessRootSignature. Error: {}", 
                    reinterpret_cast<const char*>(d3d12ErrorBlob->GetBufferPointer())
                );
            }
            else
            {
                BENZIN_CRITICAL("Failed to create BindlessRootSignature");
            }

            BENZIN_ASSERT(false);
        }

        BENZIN_D3D12_ASSERT(m_D3D12Device->CreateRootSignature(
            0,
            d3d12SerializedRootSignatureBlob->GetBufferPointer(),
            d3d12SerializedRootSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&m_D3D12BindlessRootSignature)
        ));

        detail::SetD3D12ObjectDebutName(m_D3D12BindlessRootSignature, "BindlessRootSignature", true);
    }

    ID3D12Resource* Device::CreateD3D12CommittedResource(
        D3D12_HEAP_TYPE d3d12HeapType,
        const D3D12_RESOURCE_DESC* d3d12ResourceDesc,
        const D3D12_CLEAR_VALUE* d3d12ClearValueDesc
    ) const
    {
        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(d3d12HeapType);
        const D3D12_HEAP_FLAGS d3d12HeapFlags = D3D12_HEAP_FLAG_NONE;
        const D3D12_RESOURCE_STATES d3d12InitialResourceState = d3d12HeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;
        
        ID3D12Resource* d3d12Resource = nullptr;

        BENZIN_D3D12_ASSERT(m_D3D12Device->CreateCommittedResource(
            &d3d12HeapProperties,
            d3d12HeapFlags,
            d3d12ResourceDesc,
            d3d12InitialResourceState,
            d3d12ClearValueDesc,
            IID_PPV_ARGS(&d3d12Resource)
        ));

        return d3d12Resource;
    }

} // namespace benzin
