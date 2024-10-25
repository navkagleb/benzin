#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/device.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/gpu_timer.hpp"
#include "benzin/graphics/pipeline_state_manager.hpp"
#include "benzin/graphics/resource.hpp"
#include "benzin/graphics/sampler.hpp"
#include "benzin/graphics/unified_root_signature.hpp"

namespace benzin
{

    Device::Device(const DeviceCreation& creation)
        : m_Backend{ creation.BackendRef }
    {
        ComPtr<ID3D12Device> d3d12Device;
        BenzinEnsure(::D3D12CreateDevice(m_Backend.GetDxgiMainAdapter(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device)));
        BenzinEnsure(d3d12Device->QueryInterface(&m_D3D12Device));
        SetDxObjectDebugName(m_D3D12Device, creation.DebugName);

        EnableD3D12DebugBreakOn(m_D3D12Device, true, D3D12BreakReasonFlag::Warning | D3D12BreakReasonFlag::Error | D3D12BreakReasonFlag::Corruption);

        Asserter::SetDeviceRemovedCallback([this]
        {
            const HRESULT removedReason = m_D3D12Device->GetDeviceRemovedReason();
            BenzinError(
                "\n"
                "DredMessages: {}\n"
                "CPUFrameIndex: {}, GPUFrameIndex: {}, ActiveFrameIndex: {}\n"
                "RemoveDevice was trigerred. DeviceRemovedReason: ({:#0x}) {}\n",
                GetDredMessages(m_D3D12Device),
                m_CpuFrameIndex, m_CompletedGpuFrameIndex, m_ActiveFrameIndex,
                (uint32_t)removedReason, DxgiErrorToString(removedReason)
            );

            return removedReason;
        });
        
        CheckFeaturesSupport();

        MakeUniquePtr(m_UnifiedRootSignature, *this);
        MakeUniquePtr(m_DescriptorManager, *this);
        MakeUniquePtr(m_PipelineStateManager, *this);
        MakeUniquePtr(m_GraphicsCommandQueue, *this);
        MakeUniquePtr(m_GpuTimer, *this, GpuTimer::s_MaxGpuTimerCount); // #TODO: Add more timers
    }

    Device::~Device()
    {
        m_UnifiedRootSignature.reset();
        m_DescriptorManager.reset();
        m_PipelineStateManager.reset();
        m_GraphicsCommandQueue.reset();
        m_GpuTimer.reset();

        ProcessDeferredReleaseQueues(true);

        EnableD3D12DebugBreakOn(m_D3D12Device, false, D3D12BreakReasonFlag::Warning);
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

    void Device::DeferredRelease(const Descriptor& descriptor)
    {
        BenzinAssert(descriptor.IsCpuValid());

        m_DeferredReleaseDescriptorQueue.emplace(m_CpuFrameIndex, descriptor);
    }

    void Device::DeferredRelease(const PipelineState& pso)
    {
        BenzinAssert(pso.GetD3D12PipelineState() != nullptr);

        m_DeferredReleaseResourceQueue.emplace(m_CpuFrameIndex, pso.GetD3D12PipelineState());
    }

    void Device::DeferredRelease(const Resource& resource)
    {
        BenzinAssert(resource.GetD3D12Resource() != nullptr);

        m_DeferredReleaseResourceQueue.emplace(m_CpuFrameIndex, resource.GetD3D12Resource());
    }

    void Device::ProcessDeferredReleaseQueues(bool isForceRelease)
    {
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
                .HighestShaderModel = D3D_SHADER_MODEL_6_6,
            };

            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &d3d12FeatureDataShaderModel, sizeof(d3d12FeatureDataShaderModel)));
            BenzinEnsure(d3d12FeatureDataShaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12FeatureDataShaderModel.HighestShaderModel));
        }
       
        // Ray Tracing
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 d3d12Options{};
            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12Options.RaytracingTier));
        }

        // DRED Breadcrumb
        {
            D3D12_FEATURE_DATA_EXISTING_HEAPS d3d12Options{};
            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_EXISTING_HEAPS, &d3d12Options, sizeof(d3d12Options)));

            m_Caps.IsDredSupported = d3d12Options.Supported = 1;

            BenzinTrace("Is Dred supported: {}", m_Caps.IsDredSupported);
        }

        // GPU Upload Heaps
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS16 d3d12Options{};
            BenzinEnsure(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &d3d12Options, sizeof(d3d12Options)));

            m_Caps.IsGpuUploadHeapsSupported = d3d12Options.GPUUploadHeapSupported == 1;
            BenzinTrace("Is GpuUploadHeaps supported: {}", m_Caps.IsGpuUploadHeapsSupported);

            m_Caps.IsGpuUploadHeapsSupported &= CommandLineArgs::GetBool("IsGpuUploadHeapsEnabled");
            BenzinTrace("Is GpuUploadHeaps enabled: {}", m_Caps.IsGpuUploadHeapsSupported);
        }

        BenzinTrace(Logger::s_LineSeparator);
    }

} // namespace benzin
