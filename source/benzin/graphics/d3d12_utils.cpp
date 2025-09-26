#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/d3d12_utils.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/logger.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>

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

    //

    D3D12_HEAP_PROPERTIES GetD3D12HeapProperties(D3D12_HEAP_TYPE d3d12HeapType)
    {
        return D3D12_HEAP_PROPERTIES
        {
            .Type = d3d12HeapType,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 1,
            .VisibleNodeMask = 1,
        };
    }

    D3D12_HEAP_TYPE ToD3D12HeapType(const Device& device, GpuHeapType gpuHeapType)
    {
        BenzinUnused(device);

        switch (gpuHeapType)
        {
            case GpuHeapType::Default: return D3D12_HEAP_TYPE_DEFAULT;
            case GpuHeapType::Upload: return D3D12_HEAP_TYPE_UPLOAD;
            case GpuHeapType::Readback: return D3D12_HEAP_TYPE_READBACK;
            case GpuHeapType::GpuUpload:
            {
                BenzinEnsure(device.GetCaps().IsGpuUploadHeapsSupported);
                return D3D12_HEAP_TYPE_GPU_UPLOAD;
            }
        }

        std::unreachable();
    }

    void EnableD3D12DebugLayer()
    {
        // Note: Enabling the debug layer after device creation will invalidate the active device

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

    void EnableD3D12DebugBreakOn(ID3D12Device* d3d12Device, bool isEnabled, EnumFlags<D3D12BreakReasonFlag> flags)
    {
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        BenzinD3D12Call(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)));
        
        ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
        BenzinD3D12Call(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
        
        if (flags.IsSet(D3D12BreakReasonFlag::Warning))
        {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, isEnabled);
            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, isEnabled);
        }
        
        if (flags.IsSet(D3D12BreakReasonFlag::Error))
        {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, isEnabled);
            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, isEnabled);
        }
        
        if (flags.IsSet(D3D12BreakReasonFlag::Corruption))
        {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, isEnabled);
            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, isEnabled);
        }
    }

    void ReportLiveD3D12Objects(ID3D12Device* d3d12Device)
    {
        ComPtr<ID3D12DebugDevice2> d3d12DebugDevice;
        BenzinD3D12Call(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));

        d3d12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_IGNORE_INTERNAL | D3D12_RLDO_DETAIL | D3D12_RLDO_SUMMARY);
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

    void EnableDred()
    {
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> d3d12DredSettings;
        BenzinD3D12Call(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12DredSettings)));

        d3d12DredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        d3d12DredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        d3d12DredSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }

    std::string GetDredMessages(ID3D12Device* d3d12Device)
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

}
