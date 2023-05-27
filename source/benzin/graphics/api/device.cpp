#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/device.hpp"

#include "benzin/graphics/api/backend.hpp"
#include "benzin/graphics/api/command_queue.hpp"
#include "benzin/graphics/api/pipeline_state.hpp"
#include "benzin/graphics/api/sampler.hpp"
#include "benzin/graphics/texture_loader.hpp"

namespace benzin
{
    namespace
    {

        D3D12_FILTER ConvertToD3D12TextureFilter(const Sampler::TextureFilterType& minification, const Sampler::TextureFilterType& magnification, const Sampler::TextureFilterType& mipLevel)
        {
            D3D12_FILTER d3d12Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

            switch (minification)
            {
                using enum Sampler::TextureFilterType;

                case Point:
                {
                    BENZIN_ASSERT(magnification != Anisotropic && mipLevel != Anisotropic);

                    if (magnification == Point)
                    {
                        d3d12Filter = mipLevel == Point ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                    }
                    else
                    {
                        d3d12Filter = mipLevel == Point ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
                    }

                    break;
                }
                case Linear:
                {
                    BENZIN_ASSERT(magnification != Anisotropic && mipLevel != Anisotropic);

                    if (magnification == Point)
                    {
                        d3d12Filter = mipLevel == Point ? D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT : D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
                    }
                    else
                    {
                        d3d12Filter = mipLevel == Point ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                    }

                    break;
                }
                case Anisotropic:
                {
                    BENZIN_ASSERT(magnification == Anisotropic && mipLevel == Anisotropic);

                    d3d12Filter = D3D12_FILTER_ANISOTROPIC;

                    break;
                }
                default:
                {
                    BENZIN_ERROR("Unknown TextureFilterType");
                    BENZIN_ASSERT(false);

                    break;
                }
            }

            return d3d12Filter;
        }

    } // anonymous namespace

    Device::Device(const Backend& backend)
    {
        BENZIN_HR_ASSERT(D3D12CreateDevice(backend.GetDXGIMainAdapter(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_D3D12Device)));

#if defined(BENZIN_DEBUG_BUILD)
        BreakOnD3D12Error(true);
#endif

        CreateBindlessRootSignature();

        {
            m_DescriptorManager = new DescriptorManager{ *this };
            m_TextureLoader = new TextureLoader{ *this };
        }

        {
            m_CopyCommandQueue = new CopyCommandQueue{ *this };
            m_ComputeCommandQueue = new ComputeCommandQueue{ *this };
            m_GraphicsCommandQueue = new GraphicsCommandQueue{ *this };
        }
    }

    Device::~Device()
    {
        delete m_TextureLoader;
        delete m_DescriptorManager;

        delete m_GraphicsCommandQueue;
        delete m_ComputeCommandQueue;
        delete m_CopyCommandQueue;

        dx::SafeRelease(m_D3D12BindlessRootSignature);

#if defined(BENZIN_DEBUG_BUILD)
        // Depends on m_D3D12Device
        BreakOnD3D12Error(false);
        ReportLiveObjects();
#endif

        dx::SafeRelease(m_D3D12Device);
    }

    void Device::CreateBindlessRootSignature()
    {
        // Check for support
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12OptionalFeatures{};
            BENZIN_HR_ASSERT(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12OptionalFeatures, sizeof(d3d12OptionalFeatures)));
            BENZIN_ASSERT(d3d12OptionalFeatures.ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_3);

            D3D12_FEATURE_DATA_SHADER_MODEL d3d12FeatureDataShaderModel
            {
                .HighestShaderModel{ D3D_SHADER_MODEL_6_6 }
            };

            BENZIN_HR_ASSERT(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &d3d12FeatureDataShaderModel, sizeof(d3d12FeatureDataShaderModel)));
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
                .MaxAnisotropy{ 16 },
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
                .MaxAnisotropy{ 16 },
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
                BENZIN_ERROR(
                    "Failed to create BindlessRootSignature. Error: {}", 
                    reinterpret_cast<const char*>(d3d12ErrorBlob->GetBufferPointer())
                );
            }
            else
            {
                BENZIN_ERROR("Failed to create BindlessRootSignature");
            }

            BENZIN_ASSERT(false);
        }

        BENZIN_HR_ASSERT(m_D3D12Device->CreateRootSignature(
            0,
            d3d12SerializedRootSignatureBlob->GetBufferPointer(),
            d3d12SerializedRootSignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&m_D3D12BindlessRootSignature)
        ));

        dx::SetDebugName(m_D3D12BindlessRootSignature, "Bindless");
    }

#if defined(BENZIN_DEBUG_BUILD)
    void Device::BreakOnD3D12Error(bool isBreak)
    {
        ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
        BENZIN_HR_ASSERT(m_D3D12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));

        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, isBreak);
        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, isBreak);
        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, isBreak);
    }

    void Device::ReportLiveObjects()
    {
        ComPtr<ID3D12DebugDevice2> d3d12DebugDevice;
        BENZIN_HR_ASSERT(m_D3D12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));

        d3d12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_IGNORE_INTERNAL);
    }
#endif

} // namespace benzin
