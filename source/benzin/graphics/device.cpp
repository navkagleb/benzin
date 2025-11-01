#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/device.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/fence.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/query_heap.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

namespace benzin
{

    Device::Device(const DeviceCreation& creation)
    {
        ComPtr<ID3D12Device> d3d12Device;
        BenzinD3D12Call(::D3D12CreateDevice(
            creation.m_Backend.GetDxgiMainAdapter(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&d3d12Device)));

        BenzinD3D12Call(d3d12Device->QueryInterface(&m_D3D12Device));
        SetD3DObjectDebugName(m_D3D12Device, creation.m_DebugName);

        EnableD3D12DebugBreakOn(
            m_D3D12Device,
            true,
            D3D12BreakReasonFlag::Warning | D3D12BreakReasonFlag::Error | D3D12BreakReasonFlag::Corruption);

        D3D12Asserter::SetDeviceRemovedCallback([this]
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
        MakeUniquePtr(m_GraphicsCmdQueue, *this);
        MakeUniquePtr(m_FrameFence, *this, FenceCreation{ "FrameFence", m_CompletedGpuFrameIndex });

        MakeUniquePtr(m_PersistentDefaultHeap, *this, GpuHeapCreation{ .m_DebugName = "PersistentDefaultHeap", .m_Type = GpuHeapType::Default, .m_SizeInBytes = 150_mb });
        MakeUniquePtr(m_PersistentGpuUploadHeap, *this, GpuHeapCreation{ .m_DebugName = "PersistentUploadHeap", .m_Type = GpuHeapType::GpuUpload, .m_SizeInBytes = 4_mb });
        MakeUniquePtr(m_PersistentReadbackHeap, *this, GpuHeapCreation{ .m_DebugName = "PersistentReadbackHeap", .m_Type = GpuHeapType::Readback, .m_SizeInBytes = 4_mb });

        MakeUniquePtr(m_PersistentDefaultAllocator, *m_PersistentDefaultHeap);
        MakeUniquePtr(m_PersistentGpuUploadAllocator, *m_PersistentGpuUploadHeap);
        MakeUniquePtr(m_PersistentReadbackAllocator, *m_PersistentReadbackHeap);

        MakeUniquePtr(m_ConstBufferAllocator, *this);
    }

    Device::~Device()
    {
        BenzinTraceScopeTime("Device::~Device");

        m_GraphicsCmdQueue->Flush();

        m_ConstBufferAllocator.reset();

        m_PersistentReadbackAllocator.reset();
        m_PersistentGpuUploadAllocator.reset();
        m_PersistentDefaultAllocator.reset();
        
        m_PersistentReadbackHeap.reset();
        m_PersistentGpuUploadHeap.reset();
        m_PersistentDefaultHeap.reset();

        ProcessDeferredReleaseQueues(true);

        m_GraphicsCmdQueue.reset();
        m_DescriptorManager.reset();
        m_UnifiedRootSignature.reset();

        EnableD3D12DebugBreakOn(m_D3D12Device, false, D3D12BreakReasonFlag::Warning);
        ReportLiveD3D12Objects(m_D3D12Device);

        // TODO: There is reference count due to implicit heaps of resources
        SafeReleaseD3DObject(m_D3D12Device);
    }

    uint8_t Device::GetPlaneCountFromFormat(DXGI_FORMAT dxgiFormat) const
    {
        BenzinAssert(dxgiFormat != DXGI_FORMAT_UNKNOWN);

        D3D12_FEATURE_DATA_FORMAT_INFO d3d12FormatInfo = {};
        d3d12FormatInfo.Format = dxgiFormat;

        BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &d3d12FormatInfo, sizeof(d3d12FormatInfo)));

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
        BenzinProfile();

        while (!m_DeferredReleaseResourceQueue.empty())
        {
            auto&& [cpuFrameIndex, d3d12Object] = m_DeferredReleaseResourceQueue.front();

            if (!isForceRelease && cpuFrameIndex >= m_CompletedGpuFrameIndex)
                break;

            SafeReleaseD3DObject(d3d12Object);
            m_DeferredReleaseResourceQueue.pop();
        }

        while (!m_DeferredReleaseDescriptorQueue.empty())
        {
            auto&& [cpuFrameIndex, descriptor] = m_DeferredReleaseDescriptorQueue.front();

            if (!isForceRelease && cpuFrameIndex >= m_CompletedGpuFrameIndex)
                break;

            m_DescriptorManager->FreeDescriptor(descriptor);
            m_DeferredReleaseDescriptorQueue.pop();
        }
    }

    void Device::SignalFrameFence()
    {
        BenzinProfile();

        m_CpuFrameIndex++;
        m_GraphicsCmdQueue->SignalFence(*m_FrameFence, m_CpuFrameIndex);
    }

    void Device::WaitForGpuIfNeeded()
    {
        BenzinProfile();

        m_CompletedGpuFrameIndex = m_FrameFence->GetCompletedValue();

        if (m_CpuFrameIndex - m_CompletedGpuFrameIndex < BENZIN_FRAME_COUNT)
            return;

        const auto waitForGpu = [this]
        {
            BenzinScopeProfile("Device::WaitForGpu");

            const uint64_t gpuFrameIndexToWait = m_CpuFrameIndex - BENZIN_FRAME_COUNT + 1;
            m_FrameFence->StopCurrentThreadBeforeGpuFinish(gpuFrameIndexToWait);
        };

        m_GpuWaitTime = BenzinProfileFunction(waitForGpu());

        // 'm_FrameFence' completed value may differ from 'gpuFrameIndexToWait'
        // Therefore, save 'm_FrameFence' completed value because it's may be updated during the waiting time
        m_CompletedGpuFrameIndex = m_FrameFence->GetCompletedValue();
    }

    void Device::AdvanceFrame(uint32_t activeFrameIndex)
    {
        m_ActiveFrameIndex = activeFrameIndex;
    }

    void Device::CheckFeaturesSupport()
    {
        // Dynamic Resources
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12Options{};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3);

            BenzinTrace("Device supports {}", magic_enum::enum_name(d3d12Options.ResourceBindingTier));

            D3D12_FEATURE_DATA_SHADER_MODEL d3d12FeatureDataShaderModel
            {
                .HighestShaderModel = D3D_SHADER_MODEL_6_6,
            };

            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &d3d12FeatureDataShaderModel, sizeof(d3d12FeatureDataShaderModel)));
            BenzinEnsure(d3d12FeatureDataShaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6);

            BenzinTrace("Device supports '{}'", magic_enum::enum_name(d3d12FeatureDataShaderModel.HighestShaderModel));
        }
       
        // Ray Tracing
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 d3d12Options{};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure(d3d12Options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0);

            BenzinTrace("Device supports '{}'", magic_enum::enum_name(d3d12Options.RaytracingTier));
        }

        // DRED Breadcrumb
        {
            D3D12_FEATURE_DATA_EXISTING_HEAPS d3d12Options{};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_EXISTING_HEAPS, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure((bool)d3d12Options.Supported);

            BenzinTrace("Device supports 'D3D12_FEATURE_EXISTING_HEAPS' (DRED)");
        }

        // Mesh shaders
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS9 d3d12Options{};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS9, &d3d12Options, sizeof(d3d12Options)));
            BenzinEnsure((bool)d3d12Options.MeshShaderPipelineStatsSupported);

            BenzinTrace("Device supports 'Mesh Shaders'");
        }

        // GPU Upload Heaps
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS16 d3d12Options{};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &d3d12Options, sizeof(d3d12Options)));

            m_Caps.m_IsGpuUploadHeapsSupported = d3d12Options.GPUUploadHeapSupported == 1;
            if (m_Caps.m_IsGpuUploadHeapsSupported)
            {
                m_Caps.m_IsGpuUploadHeapsSupported &= CmdLineArgs::IsGpuUploadHeapsEnabled();
                BenzinTrace("Device supports 'GPU_UPLOAD_HEAPS' (IsEnabled: {})", m_Caps.m_IsGpuUploadHeapsSupported);
            }
        }
    }

}
