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

    static void FormatToBuffer(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 d3d12DredAutoBreadcrumbsOutput, std::string& buffer)
    {
        std::format_to(std::back_inserter(buffer), "D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1\n");

        const D3D12_AUTO_BREADCRUMB_NODE1* d3d12AutoBreadcrumbNode = d3d12DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
        while (d3d12AutoBreadcrumbNode)
        {
            std::format_to(std::back_inserter(buffer), "  D3D12_AUTO_BREADCRUMB_NODE1: {}\n", (const void*)d3d12AutoBreadcrumbNode);

            if (d3d12AutoBreadcrumbNode->pCommandListDebugNameA)
            {
                std::format_to(std::back_inserter(buffer), "    D3D12 CommandList name: {}\n", d3d12AutoBreadcrumbNode->pCommandListDebugNameA);
            }

            if (d3d12AutoBreadcrumbNode->pCommandQueueDebugNameA)
            {
                std::format_to(std::back_inserter(buffer), "    D3D12 CommandQueue name: {}\n", d3d12AutoBreadcrumbNode->pCommandQueueDebugNameA);
            }

            if (d3d12AutoBreadcrumbNode->pLastBreadcrumbValue)
            {
                std::format_to(std::back_inserter(buffer), "    GPU-completed render operations: {}\n", *d3d12AutoBreadcrumbNode->pLastBreadcrumbValue);
            }

            if (d3d12AutoBreadcrumbNode->BreadcrumbCount != 0)
            {
                std::format_to(std::back_inserter(buffer), "    Number of render operations used in the command list recording: {}:\n", d3d12AutoBreadcrumbNode->BreadcrumbCount);

                const std::span<const D3D12_AUTO_BREADCRUMB_OP> d3d12AutoBreadcrumbOps{ d3d12AutoBreadcrumbNode->pCommandHistory, d3d12AutoBreadcrumbNode->BreadcrumbCount };
                for (const auto [i, d3d12AutoBreadcrumbOp] : d3d12AutoBreadcrumbOps | std::views::enumerate)
                {
                    std::format_to(std::back_inserter(buffer), "    {}: {}\n", i, magic_enum::enum_name(d3d12AutoBreadcrumbOp));
                }
            }

            if (d3d12AutoBreadcrumbNode->BreadcrumbContextsCount != 0)
            {
                std::format_to(std::back_inserter(buffer), "    Breadcrumb Contexts:\n");

                const std::span<const D3D12_DRED_BREADCRUMB_CONTEXT> d3d12BreadcrumbContexts{ d3d12AutoBreadcrumbNode->pBreadcrumbContexts, d3d12AutoBreadcrumbNode->BreadcrumbContextsCount };
                for (const auto& d3d12BreadcrumbContext : d3d12BreadcrumbContexts)
                {
                    std::format_to(std::back_inserter(buffer), "      BreadcrumbIndex: {}, Context: {}\n", d3d12BreadcrumbContext.BreadcrumbIndex, ToNarrowString(d3d12BreadcrumbContext.pContextString));
                }
            }

            d3d12AutoBreadcrumbNode = d3d12AutoBreadcrumbNode->pNext;
        }
    }

    static void FormatToBuffer(const D3D12_DRED_PAGE_FAULT_OUTPUT2& d3d12DredPageFaultOutput, std::string& buffer)
    {
        static const auto FormatToBuffer = [](const D3D12_DRED_ALLOCATION_NODE1* d3d12DREDAllocationNode, std::string_view title, std::string& buffer)
        {
            if (d3d12DREDAllocationNode)
            {
                std::format_to(std::back_inserter(buffer), "{}\n", title);
            }

            while (d3d12DREDAllocationNode)
            {
                std::format_to(std::back_inserter(buffer), "  D3D12_DRED_ALLOCATION_NODE1: {}\n", (const void*)d3d12DREDAllocationNode);

                if (d3d12DREDAllocationNode->ObjectNameA)
                {
                    std::format_to(std::back_inserter(buffer), "    D3D12 ObjectName: {}\n", d3d12DREDAllocationNode->ObjectNameA);
                }

                std::format_to(std::back_inserter(buffer), "    AllocationType: {}\n", magic_enum::enum_name(d3d12DREDAllocationNode->AllocationType));

                d3d12DREDAllocationNode = d3d12DREDAllocationNode->pNext;
            }
        };

        std::format_to(std::back_inserter(buffer), "D3D12_DRED_PAGE_FAULT_OUTPUT2\n");
        std::format_to(std::back_inserter(buffer), "PageFaultVA: {:#x}\n", d3d12DredPageFaultOutput.PageFaultVA);

        FormatToBuffer(d3d12DredPageFaultOutput.pHeadExistingAllocationNode, "HeadExistingAllocationNode", buffer);
        FormatToBuffer(d3d12DredPageFaultOutput.pHeadRecentFreedAllocationNode, "HeadRecentFreedAllocationNode", buffer);
    }

    static std::string GetDredMessages(ID3D12Device* d3d12Device)
    {
        std::string buffer;
        buffer.reserve(1_mb);

        ComPtr<ID3D12DeviceRemovedExtendedData2> d3d12Dred;
        BenzinD3D12Call(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12Dred)));

        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 d3d12DredAutoBreadcrumbsOutput;
        BenzinD3D12Call(d3d12Dred->GetAutoBreadcrumbsOutput1(&d3d12DredAutoBreadcrumbsOutput));

        D3D12_DRED_PAGE_FAULT_OUTPUT2 d3d12DredPageFaultOutput;
        BenzinD3D12Call(d3d12Dred->GetPageFaultAllocationOutput2(&d3d12DredPageFaultOutput));

        const D3D12_DRED_DEVICE_STATE d3d12DredDeviceState = d3d12Dred->GetDeviceState();
        std::format_to(std::back_inserter(buffer), "D3D12_DRED_DEVICE_STATE: {}\n", magic_enum::enum_name(d3d12DredDeviceState));

        FormatToBuffer(d3d12DredAutoBreadcrumbsOutput, buffer);
        FormatToBuffer(d3d12DredPageFaultOutput, buffer);

        return buffer;
    }

    std::string_view DxgiErrorToString(HRESULT hr)
    {
        switch (hr)
        {
            case DXGI_ERROR_DEVICE_HUNG: return BenzinStringify(DXGI_ERROR_DEVICE_HUNG);
            case DXGI_ERROR_DEVICE_REMOVED: return BenzinStringify(DXGI_ERROR_DEVICE_REMOVED);
            case DXGI_ERROR_DEVICE_RESET: return BenzinStringify(DXGI_ERROR_DEVICE_RESET);
            case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return BenzinStringify(DXGI_ERROR_DRIVER_INTERNAL_ERROR);
            case DXGI_ERROR_INVALID_CALL: return BenzinStringify(DXGI_ERROR_INVALID_CALL);

            case DXGI_ERROR_ACCESS_DENIED: return BenzinStringify(DXGI_ERROR_ACCESS_DENIED);
        }

        return std::string_view{};
    }

    //

    Device::Device(const DeviceCreation& creation)
    {
#if BENZIN_IS_DEBUG_BUILD
        {
            ComPtr<ID3D12Debug5> d3d12Debug;
            BenzinD3D12Call(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));

            d3d12Debug->EnableDebugLayer();
            d3d12Debug->SetEnableGPUBasedValidation(CmdLineArgs::IsGpuValidationEnabled());
            d3d12Debug->SetEnableSynchronizedCommandQueueValidation(CmdLineArgs::IsSynchronizedCommandQueueValidationEnabled());
            d3d12Debug->SetEnableAutoName(true);

            BenzinTrace("D3D12DebugLayer enabled");
            BenzinTrace("GPUBasedValidation enabled: {}", CmdLineArgs::IsGpuValidationEnabled());
            BenzinTrace("SynchronizedCommandQueueValidation enabled: {}", CmdLineArgs::IsSynchronizedCommandQueueValidationEnabled());
        }
#endif

        ComPtr<ID3D12Device> d3d12Device;
        BenzinD3D12Call(::D3D12CreateDevice(
            creation.m_Backend.GetDxgiMainAdapter(),
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&d3d12Device)));

        BenzinD3D12Call(d3d12Device->QueryInterface(&m_D3D12Device));
        SetD3DObjectDebugName(m_D3D12Device, creation.m_DebugName);

#if BENZIN_IS_DEBUG_BUILD
        {
            ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
            BenzinD3D12Call(m_D3D12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
            BenzinD3D12Call(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
            BenzinD3D12Call(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
            BenzinD3D12Call(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
        }

        {
            ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> d3d12DredSettings;
            BenzinD3D12Call(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12DredSettings)));

            d3d12DredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            d3d12DredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            d3d12DredSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }
#endif

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
                (uint32_t)removedReason, DxgiErrorToString(removedReason));

            return removedReason;
        });
        
        CheckFeaturesSupport();

        MakeUniquePtr(m_UnifiedRootSignature, *this);
        MakeUniquePtr(m_DescriptorManager, *this);
        MakeUniquePtr(m_GraphicsCmdQueue, *this);
        MakeUniquePtr(m_FrameFence, *this, FenceCreation{ "FrameFence", m_CompletedGpuFrameIndex });

        MakeUniquePtr(m_PersistentDefaultHeap, *this, GpuHeapCreation{ .m_DebugName = "Device::PersistentDefaultHeap", .m_Type = GpuHeapType::Default, .m_SizeInBytes = 600_mb });
        MakeUniquePtr(m_PersistentGpuUploadHeap, *this, GpuHeapCreation{ .m_DebugName = "Device::PersistentUploadHeap", .m_Type = GpuHeapType::GpuUpload, .m_SizeInBytes = 4_mb });
        MakeUniquePtr(m_PersistentReadbackHeap, *this, GpuHeapCreation{ .m_DebugName = "Device::PersistentReadbackHeap", .m_Type = GpuHeapType::Readback, .m_SizeInBytes = 4_mb });
        MakeUniquePtr(m_ResDependentHeap, *this, GpuHeapCreation{ .m_DebugName = "Device::ResDependentHeap", .m_Type = GpuHeapType::Default, .m_SizeInBytes = 150_mb });

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

#if BENZIN_IS_DEBUG_BUILD
        {
            ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
            BenzinD3D12Call(m_D3D12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
            BenzinD3D12Call(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false));
        }

        {
            ComPtr<ID3D12DebugDevice2> d3d12DebugDevice;
            BenzinD3D12Call(m_D3D12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));
            BenzinD3D12Call(d3d12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL));
        }
#endif

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
