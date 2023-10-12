#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/device.hpp"

#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/pipeline_state.hpp"
#include "benzin/graphics/sampler.hpp"
#include "benzin/graphics/texture_loader.hpp"

namespace benzin
{
    namespace
    {

        D3D12_FILTER ToD3D12TextureFilter(const TextureFilterType& minification, const TextureFilterType& magnification, const TextureFilterType& mipLevel)
        {
            D3D12_FILTER d3d12Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

            switch (minification)
            {
                using enum TextureFilterType;

                case Point:
                {
                    BenzinAssert(magnification != Anisotropic && mipLevel != Anisotropic);

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
                    BenzinAssert(magnification != Anisotropic && mipLevel != Anisotropic);

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
                    BenzinAssert(magnification == Anisotropic && mipLevel == Anisotropic);

                    d3d12Filter = D3D12_FILTER_ANISOTROPIC;

                    break;
                }
                default:
                {
                    BenzinAssert(false);
                    break;
                }
            }

            return d3d12Filter;
        }

        D3D12_STATIC_SAMPLER_DESC ToD3D12StaticSamplerDesc(const StaticSampler& staticSampler)
        {
            return D3D12_STATIC_SAMPLER_DESC
            {
                .Filter = ToD3D12TextureFilter(staticSampler.Sampler.Minification, staticSampler.Sampler.Magnification, staticSampler.Sampler.MipLevel),
                .AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.Sampler.AddressU),
                .AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.Sampler.AddressV),
                .AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(staticSampler.Sampler.AddressW),
                .MipLODBias = staticSampler.MipLODBias,
                .MaxAnisotropy = staticSampler.MaxAnisotropy,
                .ComparisonFunc = static_cast<D3D12_COMPARISON_FUNC>(staticSampler.ComparisonFunction),
                .BorderColor = static_cast<D3D12_STATIC_BORDER_COLOR>(staticSampler.BorderColor),
                .MinLOD = 0.0f,
                .MaxLOD = D3D12_FLOAT32_MAX,
                .ShaderRegister = staticSampler.ShaderRegister.Index,
                .RegisterSpace = staticSampler.ShaderRegister.Space,
                .ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(staticSampler.ShaderVisibility),
            };
        }

    } // anonymous namespace

    Device::Device(const Backend& backend)
    {
        ComPtr<ID3D12Device> d3d12Device;
        BenzinAssert(D3D12CreateDevice(backend.GetDXGIMainAdapter(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device)));
        BenzinAssert(d3d12Device->QueryInterface(&m_D3D12Device));

#if BENZIN_IS_DEBUG_BUILD
        BreakOnD3D12Error(true);
#endif
        
        CheckFeaturesSupport();
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

        SafeUnknownRelease(m_D3D12BindlessRootSignature);

#if BENZIN_IS_DEBUG_BUILD
        // Depends on m_D3D12Device
        BreakOnD3D12Error(false);
        ReportLiveObjects();
#endif

        // TODO: There is reference count due to implicit heaps of resources
        SafeUnknownRelease(m_D3D12Device);
    }

    uint8_t Device::GetPlaneCountFromFormat(GraphicsFormat format) const
    {
        BenzinAssert(format != GraphicsFormat::Unknown);

        D3D12_FEATURE_DATA_FORMAT_INFO d3d12FormatInfo{ .Format = static_cast<DXGI_FORMAT>(format) };
        BenzinAssert(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &d3d12FormatInfo, sizeof(d3d12FormatInfo)));

        return d3d12FormatInfo.PlaneCount;
    }

    void Device::CheckFeaturesSupport()
    {
        // Dynamic Resources
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12Options{};
            BenzinAssert(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12Options.ResourceBindingTier));

            D3D12_FEATURE_DATA_SHADER_MODEL d3d12FeatureDataShaderModel
            {
                .HighestShaderModel{ D3D_SHADER_MODEL_6_6 }
            };

            BenzinAssert(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &d3d12FeatureDataShaderModel, sizeof(d3d12FeatureDataShaderModel)));
            BenzinEnsure(d3d12FeatureDataShaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12FeatureDataShaderModel.HighestShaderModel));
        }
       
        // Ray Tracing
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 d3d12Options;
            BenzinAssert(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12Options.RaytracingTier));
        }

        // Mesh Shaders
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS7 d3d12Options;
            BenzinAssert(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12Options.MeshShaderTier));
        }
    }

    void Device::CreateBindlessRootSignature()
    {
        const D3D12_ROOT_PARAMETER1 d3d12RootParameter
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants
            {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
                .Num32BitValues = 32,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        constexpr uint32_t staticSamplerCount = 7;
        const std::array<StaticSampler, staticSamplerCount> staticSamplers
        {
            StaticSampler::GetPointWrap({ 0, 0 }),
            StaticSampler::GetPointClamp({ 1, 0 }),
            StaticSampler::GetLinearWrap({ 2, 0 }),
            StaticSampler::GetLinearClamp({ 3, 0 }),
            StaticSampler::GetAnisotropicWrap({ 4, 0 }),
            StaticSampler::GetAnisotropicClamp({ 5, 0 }),
            StaticSampler::GetDefaultForShadow({ 6, 0 }),
        };

        std::array<D3D12_STATIC_SAMPLER_DESC, staticSamplerCount> d3d12StaticSamplerDescs;
        for (size_t i = 0; i < staticSamplerCount; ++i)
        {
            d3d12StaticSamplerDescs[i] = ToD3D12StaticSamplerDesc(staticSamplers[i]);
        }

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12RootSignatureDesc
        {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1
            {
                .NumParameters = 1,
                .pParameters = &d3d12RootParameter,
                .NumStaticSamplers = staticSamplerCount,
                .pStaticSamplers = d3d12StaticSamplerDescs.data(),
                .Flags
                { 
                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | 
                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
                }
            }
        };

        ComPtr<ID3DBlob> d3d12Blob;
        ComPtr<ID3DBlob> d3d12Error;

        const HRESULT result = D3D12SerializeVersionedRootSignature(&d3d12RootSignatureDesc, &d3d12Blob, &d3d12Error);
        if (FAILED(result))
        {
            BenzinError("Failed to create BindlessRootSignature");

            if (d3d12Error && d3d12Error->GetBufferSize() != 0)
            {
                BenzinError("Error: {}", reinterpret_cast<const char*>(d3d12Error->GetBufferPointer()));
            }

            BenzinAssert(false);
        }

        BenzinAssert(m_D3D12Device->CreateRootSignature(
            0,
            d3d12Blob->GetBufferPointer(),
            d3d12Blob->GetBufferSize(),
            IID_PPV_ARGS(&m_D3D12BindlessRootSignature)
        ));

        SetD3D12ObjectDebugName(m_D3D12BindlessRootSignature, "Bindless");
    }

#if BENZIN_IS_DEBUG_BUILD
    void Device::BreakOnD3D12Error(bool isBreak)
    {
        ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
        BenzinAssert(m_D3D12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));

        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, isBreak);
        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, isBreak);
        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, isBreak);
    }

    void Device::ReportLiveObjects()
    {
        ComPtr<ID3D12DebugDevice2> d3d12DebugDevice;
        BenzinAssert(m_D3D12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));

        d3d12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL);
    }
#endif

} // namespace benzin
