#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/device.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/d3d12_debug.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/fence.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/query_heap.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

namespace benzin
{

    Device::Device(std::string_view debugName, const Backend& backend)
    {
        D3D12Debug::EnableD3D12DebugLayer();

        ComPtr<ID3D12Device> d3d12Device;
        BenzinD3D12Call(::D3D12CreateDevice(
            backend.GetDxgiMainAdapter(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&d3d12Device)));

        BenzinD3D12Call(d3d12Device->QueryInterface(&m_D3D12Device));
        SetD3DObjectDebugName(m_D3D12Device, debugName);

        D3D12Debug::EnableD3D12DebugMessages(m_D3D12Device);
        D3D12Debug::EnableDred();

        D3D12Debug::SetDeviceRemovedCallback([this](HRESULT code)
        {
            if (code != DXGI_ERROR_DEVICE_REMOVED && code != DXGI_ERROR_DEVICE_HUNG && code != DXGI_ERROR_DEVICE_RESET)
                return code;

            return m_D3D12Device->GetDeviceRemovedReason();
        });

        D3D12Debug::SetDeviceRemovedCallback([this](HRESULT code)
        {
            const std::string dredMessages = D3D12Debug::ProcessDredMessages(m_D3D12Device);

            BenzinError(
                "\n"
                "CpuFrameIndex: {}, GpuFrameIndex: {}, ActiveFrameIndex: {}\n"
                "RemoveDevice was trigerred. DeviceRemovedReason: ({:#0x}) {}\n"
                "DRED Messages: {}\n",
                dredMessages,
                m_CpuFrameIndex,
                m_CompletedGpuFrameIndex,
                m_ActiveFrameIndex,
                (uint32_t)code,
                D3D12Debug::DxgiErrorToString(code));
        });

        CheckFeaturesSupport();

        MakeUniquePtr(m_UnifiedRootSignature, *this);
        MakeUniquePtr(m_DescriptorManager, *this);
        MakeUniquePtr(m_GraphicsCmdQueue, *this);
        MakeUniquePtr(m_FrameFence, *this, FenceCreation{ "FrameFence", m_CompletedGpuFrameIndex });

        MakeUniquePtr(m_PersistentDefaultHeap, *this, GpuHeapCreation{ .m_DebugName = "Device::PersistentDefaultHeap", .m_Type = GpuHeapType::Default, .m_SizeInBytes = 650_mb });
        MakeUniquePtr(m_PersistentGpuUploadHeap, *this, GpuHeapCreation{ .m_DebugName = "Device::PersistentGpuUploadHeap", .m_Type = GpuHeapType::GpuUpload, .m_SizeInBytes = 50_mb });
        MakeUniquePtr(m_PersistentReadbackHeap, *this, GpuHeapCreation{ .m_DebugName = "Device::PersistentReadbackHeap", .m_Type = GpuHeapType::Readback, .m_SizeInBytes = 4_mb });
        MakeUniquePtr(m_ResDependentHeap, *this, GpuHeapCreation{ .m_DebugName = "Device::ResDependentHeap", .m_Type = GpuHeapType::Default, .m_SizeInBytes = 250_mb });

        MakeUniquePtr(m_PersistentDefaultAllocator, *m_PersistentDefaultHeap);
        MakeUniquePtr(m_PersistentGpuUploadAllocator, *m_PersistentGpuUploadHeap);
        MakeUniquePtr(m_PersistentReadbackAllocator, *m_PersistentReadbackHeap);
        MakeUniquePtr(m_ResDependentAllocator, *m_ResDependentHeap);

        MakeUniquePtr(m_ConstBufferAllocator, *this, (uint32_t)2_mb);
    }

    Device::~Device()
    {
        BenzinTraceScopeTime("Device::~Device");

        m_GraphicsCmdQueue->Flush();

        m_ConstBufferAllocator.reset();

        m_ResDependentAllocator.reset();
        m_PersistentReadbackAllocator.reset();
        m_PersistentGpuUploadAllocator.reset();
        m_PersistentDefaultAllocator.reset();

        m_ResDependentHeap.reset();
        m_PersistentReadbackHeap.reset();
        m_PersistentGpuUploadHeap.reset();
        m_PersistentDefaultHeap.reset();

        ProcessDeferredReleaseQueues(true);

        m_FrameFence.reset();
        m_GraphicsCmdQueue.reset();
        m_DescriptorManager.reset();
        m_UnifiedRootSignature.reset();

        D3D12Debug::ReportLiveD3D12Objects(m_D3D12Device);
        D3D12Debug::SetDeviceRemovedCallback({});
        D3D12Debug::SetValidateReturnCodeCallback({});

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
        m_GpuWaitTime = {};

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
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12Options = {};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12Options, sizeof(d3d12Options)));

            BenzinTrace("Device support for 'Resource Binding': {}", magic_enum::enum_name(d3d12Options.ResourceBindingTier));
            BenzinEnsure(d3d12Options.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3);

            D3D12_FEATURE_DATA_SHADER_MODEL d3d12FeatureDataShaderModel = {};
            d3d12FeatureDataShaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_6;
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &d3d12FeatureDataShaderModel, sizeof(d3d12FeatureDataShaderModel)));

            BenzinTrace("Device support for 'Shader Model': {}", magic_enum::enum_name(d3d12FeatureDataShaderModel.HighestShaderModel));
            BenzinEnsure(d3d12FeatureDataShaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_6);
        }

        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 d3d12Options = {};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &d3d12Options, sizeof(d3d12Options)));

            BenzinTrace("Device support for 'Raytracing': {}", magic_enum::enum_name(d3d12Options.RaytracingTier));
            BenzinEnsure(d3d12Options.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0);
        }

        {
            D3D12_FEATURE_DATA_EXISTING_HEAPS d3d12Options = {};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_EXISTING_HEAPS, &d3d12Options, sizeof(d3d12Options)));

            BenzinTrace("Device support for 'Existing Heaps (DRED)': {}", d3d12Options.Supported);
            BenzinEnsure((bool)d3d12Options.Supported != 0);
        }

        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS7 d3d12Options7 = {};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &d3d12Options7, sizeof(d3d12Options7)));

            BenzinTrace("Device support for 'Mesh shader': {}", magic_enum::enum_name(d3d12Options7.MeshShaderTier));
            BenzinEnsure(d3d12Options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED);

            D3D12_FEATURE_DATA_D3D12_OPTIONS9 d3d12Options9 = {};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS9, &d3d12Options9, sizeof(d3d12Options9)));

            BenzinTrace("Device support for 'Mesh Pipeline Stats': {}", d3d12Options9.MeshShaderPipelineStatsSupported);
            BenzinEnsure(d3d12Options9.MeshShaderPipelineStatsSupported != 0);
        }

        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS16 d3d12Options = {};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &d3d12Options, sizeof(d3d12Options)));

            m_Caps.m_IsGpuUploadHeapsSupported = d3d12Options.GPUUploadHeapSupported == 1;
            m_Caps.m_IsGpuUploadHeapsSupported &= CmdLineArgs::IsGpuUploadHeapsEnabled();

            BenzinTrace(
                "Device support for 'GPU Upload Heaps': {} (enabled: {})",
                d3d12Options.GPUUploadHeapSupported,
                m_Caps.m_IsGpuUploadHeapsSupported);
        }

        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS21 d3d12Options = {};
            BenzinD3D12Call(m_D3D12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS21, &d3d12Options, sizeof(d3d12Options)));

            BenzinTrace("Device support for 'Work Graphs': {}", magic_enum::enum_name(d3d12Options.WorkGraphsTier));
        }
    }

}
