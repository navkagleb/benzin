#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/device.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/gpu_timer.hpp"
#include "benzin/graphics/pipeline_state_manager.hpp"
#include "benzin/graphics/sampler.hpp"

namespace benzin
{

    using UnifiedD3D12RootParameters = EnumArray<D3D12_ROOT_PARAMETER1, UnifiedRootParameter>;

    static UnifiedD3D12RootParameters CreateUnifiedD3D12RootParamers()
    {
        UnifiedD3D12RootParameters d3d12RootParamers;

        uint32_t constantBufferSpaceIndex = 0;

        d3d12RootParamers[+UnifiedRootParameter::RootConstantBuffer] = D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants
            {
                .ShaderRegister = 0,
                .RegisterSpace = constantBufferSpaceIndex++,
                .Num32BitValues = 32,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        d3d12RootParamers[+UnifiedRootParameter::FrameConstantBuffer] = D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor
            {
                .ShaderRegister = 0,
                .RegisterSpace = constantBufferSpaceIndex++,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        d3d12RootParamers[+UnifiedRootParameter::RenderPassConstantBuffer] = D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor
            {
                .ShaderRegister = 0,
                .RegisterSpace = constantBufferSpaceIndex++,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        d3d12RootParamers[+UnifiedRootParameter::TopLevelAs] = D3D12_ROOT_PARAMETER1
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor
            {
                .ShaderRegister = 0,
                .RegisterSpace = 0,
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        return d3d12RootParamers;
    }

    static D3D12_FILTER ToD3D12TextureFilter(const TextureFilterType& minification, const TextureFilterType& magnification, const TextureFilterType& mipLevel)
    {
        switch (minification)
        {
            using enum TextureFilterType;

            case Point:
            {
                BenzinAssert(magnification != Anisotropic && mipLevel != Anisotropic);
                return magnification == Point
                    ? mipLevel == Point ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR
                    : mipLevel == Point ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
            }
            case Linear:
            {
                BenzinAssert(magnification != Anisotropic && mipLevel != Anisotropic);
                return magnification == Point 
                    ? mipLevel == Point ? D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT : D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR
                    : mipLevel == Point ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            }
            case Anisotropic:
            {
                BenzinAssert(magnification == Anisotropic && mipLevel == Anisotropic);
                return D3D12_FILTER_ANISOTROPIC;
            }
        }

        std::unreachable();
    }

    static D3D12_STATIC_SAMPLER_DESC ToD3D12StaticSamplerDesc(const StaticSampler& staticSampler)
    {
        return D3D12_STATIC_SAMPLER_DESC
        {
            .Filter = ToD3D12TextureFilter(staticSampler.Sampler.Minification, staticSampler.Sampler.Magnification, staticSampler.Sampler.MipLevel),
            .AddressU = (D3D12_TEXTURE_ADDRESS_MODE)staticSampler.Sampler.AddressU,
            .AddressV = (D3D12_TEXTURE_ADDRESS_MODE)staticSampler.Sampler.AddressV,
            .AddressW = (D3D12_TEXTURE_ADDRESS_MODE)staticSampler.Sampler.AddressW,
            .MipLODBias = staticSampler.MipLODBias,
            .MaxAnisotropy = staticSampler.MaxAnisotropy,
            .ComparisonFunc = (D3D12_COMPARISON_FUNC)staticSampler.ComparisonFunction,
            .BorderColor = (D3D12_STATIC_BORDER_COLOR)staticSampler.BorderColor,
            .MinLOD = 0.0f,
            .MaxLOD = D3D12_FLOAT32_MAX,
            .ShaderRegister = staticSampler.ShaderRegister.Index,
            .RegisterSpace = staticSampler.ShaderRegister.Space,
            .ShaderVisibility = (D3D12_SHADER_VISIBILITY)staticSampler.ShaderVisibility,
        };
    }

    //

    Device::Device(const DeviceCreation& creation)
        : m_Backend{ creation.BackendRef }
    {
        EnableDred();

        ComPtr<ID3D12Device> dx12Device;
        BenzinEnsure(::D3D12CreateDevice(m_Backend.GetDxgiMainAdapter(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&dx12Device)));
        BenzinEnsure(dx12Device->QueryInterface(&m_D3D12Device));
        SetDxObjectDebugName(m_D3D12Device, creation.DebugName);

        EnableD3D12DebugBreakOn(m_D3D12Device, true, D3D12BreakReasonFlag::Warning | D3D12BreakReasonFlag::Error | D3D12BreakReasonFlag::Corruption);

        Asserter::SetDeviceRemovedCallback([this]
        {
            const HRESULT removedReason = m_D3D12Device->GetDeviceRemovedReason();
            BenzinError(
                "\n"
                "{}\n"
                "CPUFrameIndex: {}, GPUFrameIndex: {}, ActiveFrameIndex: {}\n"
                "RemoveDevice was trigerred. DeviceRemovedReason: ({:#0x}) {}\n",
                GetDredMessages(m_D3D12Device),
                m_CpuFrameIndex, m_CompletedGpuFrameIndex, m_ActiveFrameIndex,
                (uint32_t)removedReason, DxgiErrorToString(removedReason)
            );

            return removedReason;
        });
        
        CheckFeaturesSupport();
        CreateUnifiedRootSignature();

        MakeUniquePtr(m_DescriptorManager, *this);
        MakeUniquePtr(m_PipelineStateManager, *this);
        MakeUniquePtr(m_GraphicsCommandQueue, *this);
        MakeUniquePtr(m_GpuTimer, *this, 32); // #TODO: Add more timers
    }

    Device::~Device()
    {
        m_DescriptorManager.reset();
        m_PipelineStateManager.reset();
        m_GraphicsCommandQueue.reset();
        m_GpuTimer.reset();

        BenzinSafeDxObjectRelease(m_D3D12UnifiedRootSignature);

        ProcessDeferredReleaseQueues(true);

        EnableD3D12DebugBreakOn(m_D3D12Device, false, { D3D12BreakReasonFlag::Warning });
        ReportLiveD3D12Objects(m_D3D12Device);

        // TODO: There is reference count due to implicit heaps of resources
        BenzinSafeDxObjectRelease(m_D3D12Device);
    }

    uint8_t Device::GetPlaneCountFromFormat(GraphicsFormat format) const
    {
        BenzinAssert(format != GraphicsFormat::Unknown);

        D3D12_FEATURE_DATA_FORMAT_INFO d3d12FormatInfo{ .Format = (DXGI_FORMAT)format };
        BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &d3d12FormatInfo, sizeof(d3d12FormatInfo)));

        return d3d12FormatInfo.PlaneCount;
    }

    void Device::DeferredRelease(ID3D12Object* d3d12Object)
    {
        BenzinAssert(d3d12Object != nullptr);

        m_DeferredReleaseResourceQueue.emplace(m_CpuFrameIndex, d3d12Object);
    }

    void Device::DeferredRelease(const Descriptor& descriptor)
    {
        BenzinAssert(descriptor.IsCpuValid());

        m_DeferredReleaseDescriptorQueue.emplace(m_CpuFrameIndex, descriptor);
    }

    void Device::ProcessDeferredReleaseQueues(bool isForceRelease)
    {
        if (!IsValidIndex(m_CompletedGpuFrameIndex))
        {
            return;
        }

        while (!m_DeferredReleaseResourceQueue.empty())
        {
            auto&& [cpuFrameIndex, d3d12Object] = m_DeferredReleaseResourceQueue.front();

            if (!isForceRelease && cpuFrameIndex >= m_CompletedGpuFrameIndex)
            {
                break;
            }

            BenzinSafeDxObjectRelease(d3d12Object);
            m_DeferredReleaseResourceQueue.pop();
        }

        while (!m_DeferredReleaseDescriptorQueue.empty())
        {
            auto&& [cpuFrameIndex, descriptor] = m_DeferredReleaseDescriptorQueue.front();

            if (!isForceRelease && cpuFrameIndex >= m_CompletedGpuFrameIndex)
            {
                break;
            }

            m_DescriptorManager->FreeDescriptor(descriptor);
            m_DeferredReleaseDescriptorQueue.pop();
        }
    }

    void Device::CheckFeaturesSupport()
    {
        // Dynamic Resources
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12Options{};
            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12Options.ResourceBindingTier));

            D3D12_FEATURE_DATA_SHADER_MODEL d3d12FeatureDataShaderModel
            {
                .HighestShaderModel{ D3D_SHADER_MODEL_6_6 }
            };

            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &d3d12FeatureDataShaderModel, sizeof(d3d12FeatureDataShaderModel)));
            BenzinEnsure(d3d12FeatureDataShaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12FeatureDataShaderModel.HighestShaderModel));
        }
       
        // Ray Tracing
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 d3d12Options;
            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12Options.RaytracingTier));
        }

        // DRED Breadcrumb
        {
            D3D12_FEATURE_DATA_EXISTING_HEAPS d3d12Options;
            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_EXISTING_HEAPS, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.Supported == 1);

            BenzinTrace("Device supports D3D12_FEATURE_EXISTING_HEAPS");
        }

        // GPU Upload Heaps
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS16 d3d12Options;
            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &d3d12Options, sizeof(d3d12Options)));

            m_IsGpuUploadHeapsSupported = d3d12Options.GPUUploadHeapSupported == 1;
            BenzinTrace("Is GpuUploadHeaps supported: {}", m_IsGpuUploadHeapsSupported);

            m_IsGpuUploadHeapsSupported &= CommandLineArgs::g_IsGpuUploadHeapsEnabled;
            BenzinTrace("Is GpuUploadHeaps enabled: {}", m_IsGpuUploadHeapsSupported);
        }
    }

    void Device::CreateUnifiedRootSignature()
    {
        const auto d3d12RootParameters = CreateUnifiedD3D12RootParamers();

        uint32_t samplerSpaceIndex = 0;
        const auto d3d12StaticSamplerDescs = std::to_array(
        {
            ToD3D12StaticSamplerDesc(StaticSampler::GetPointWrap({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetPointClamp({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetLinearWrap({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetLinearClamp({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetAnisotropicWrap({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler::GetAnisotropicClamp({ 0, samplerSpaceIndex++ })),
            ToD3D12StaticSamplerDesc(StaticSampler
            {
                .Sampler
                {
                    .Minification = TextureFilterType::Point,
                    .Magnification = TextureFilterType::Point,
                    .MipLevel = TextureFilterType::Point,
                    .AddressU = TextureAddressMode::Border,
                    .AddressV = TextureAddressMode::Border,
                    .AddressW = TextureAddressMode::Border,
                },
                .BorderColor = TextureBorderColor::TransparentBlack,
                .ShaderRegister{ 0, samplerSpaceIndex++ },
            }),
        });

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3d12RootSignatureDesc
        {
            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1
            {
                .NumParameters = (uint32_t)d3d12RootParameters.size(),
                .pParameters = d3d12RootParameters.data(),
                .NumStaticSamplers = (uint32_t)d3d12StaticSamplerDescs.size(),
                .pStaticSamplers = d3d12StaticSamplerDescs.data(),
                .Flags
                {
                    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
                },
            }
        };

        ComPtr<ID3DBlob> d3d12Blob;
        ComPtr<ID3DBlob> d3d12Error;
        const HRESULT hr = ::D3D12SerializeVersionedRootSignature(&d3d12RootSignatureDesc, &d3d12Blob, &d3d12Error);
        if (FAILED(hr))
        {
            BenzinEnsure(hr, "Failed to Serialize RootSignature. Error: {}", (const char*)d3d12Error->GetBufferPointer());
        }

        BenzinEnsure(m_D3D12Device->CreateRootSignature(
            0,
            d3d12Blob->GetBufferPointer(),
            d3d12Blob->GetBufferSize(),
            IID_PPV_ARGS(&m_D3D12UnifiedRootSignature)
        ));

        SetDxObjectDebugName(m_D3D12UnifiedRootSignature, "UnifiedRootSignature");
    }

} // namespace benzin
