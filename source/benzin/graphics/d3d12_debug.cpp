#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/d3d12_debug.hpp>

#include <benzin/core/cmd_line_args.hpp>

namespace benzin
{

    static D3D12Debug::ValidateReturnCodeCallback g_ValidateReturnCodeCallback;
    static D3D12Debug::DeviceRemovedCallback g_DeviceRemovedCallback;

    void D3D12Debug::SetValidateReturnCodeCallback(ValidateReturnCodeCallback&& callback)
    {
        g_ValidateReturnCodeCallback = std::move(callback);
    }

    void D3D12Debug::SetDeviceRemovedCallback(DeviceRemovedCallback&& callback)
    {
        g_DeviceRemovedCallback = std::move(callback);
    }

    HRESULT D3D12Debug::ValidateReturnCode(HRESULT code)
    {
        if (!g_ValidateReturnCodeCallback)
            return code;

        return g_ValidateReturnCodeCallback(code);
    }

    void D3D12Debug::TriggerRemoveDevice(HRESULT code)
    {
        if (!g_DeviceRemovedCallback)
            return;

        g_DeviceRemovedCallback(code);
    }

    std::string D3D12Debug::GetMessageFromReturnCode(HRESULT code)
    {
        const _com_error comError{ code };
        const std::string_view comErrorMessage = comError.ErrorMessage();

        return std::format(
            "HRESULT: ({:#0x}) {}, ComErrorMessage: {}",
            (uint32_t)code,
            DxgiErrorToString(code),
            comErrorMessage);
    }

    std::string_view D3D12Debug::DxgiErrorToString(HRESULT code)
    {
        switch (code)
        {
            case DXGI_ERROR_DEVICE_HUNG: return BenzinStringify(DXGI_ERROR_DEVICE_HUNG);
            case DXGI_ERROR_DEVICE_REMOVED: return BenzinStringify(DXGI_ERROR_DEVICE_REMOVED);
            case DXGI_ERROR_DEVICE_RESET: return BenzinStringify(DXGI_ERROR_DEVICE_RESET);
            case DXGI_ERROR_DRIVER_INTERNAL_ERROR: return BenzinStringify(DXGI_ERROR_DRIVER_INTERNAL_ERROR);
            case DXGI_ERROR_INVALID_CALL: return BenzinStringify(DXGI_ERROR_INVALID_CALL);

            case DXGI_ERROR_ACCESS_DENIED: return BenzinStringify(DXGI_ERROR_ACCESS_DENIED);
        }

        BenzinAssert(false);
        return {};
    }

    void D3D12Debug::EnableD3D12DebugLayer()
    {
#if BENZIN_DEBUG_BUILD_ENABLED
        ComPtr<ID3D12Debug5> d3d12Debug;
        if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug))))
        {
            BenzinError("Failed to enable D3D12DebugLayer");
            return;
        }

        d3d12Debug->SetEnableGPUBasedValidation(CmdLineArgs::IsGpuValidationEnabled());
        d3d12Debug->SetEnableSynchronizedCommandQueueValidation(CmdLineArgs::IsSynchronizedCommandQueueValidationEnabled());
        d3d12Debug->SetEnableAutoName(true);
        d3d12Debug->EnableDebugLayer();

        BenzinTrace("D3D12DebugLayer enabled");
        BenzinTrace("GPUBasedValidation enabled: {}", CmdLineArgs::IsGpuValidationEnabled());
        BenzinTrace("SynchronizedCommandQueueValidation enabled: {}", CmdLineArgs::IsSynchronizedCommandQueueValidationEnabled());
#endif
    }

    void D3D12Debug::EnableD3D12DebugMessages(ID3D12Device* d3d12Device)
    {
        BenzinUnused(d3d12Device);

#if BENZIN_DEBUG_BUILD_ENABLED
        ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
        if (FAILED(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue))))
        {
            BenzinError("Failed to enable D3D12 messages on InfoQueue");
            return;
        }

        BenzinD3D12Call(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
        BenzinD3D12Call(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
        BenzinD3D12Call(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
#endif
    }

    void D3D12Debug::EnableDred()
    {
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> d3d12DredSettings;
        if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12DredSettings))))
        {
            BenzinError("Failed to enable DRED");
            return;
        }

        d3d12DredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        d3d12DredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        d3d12DredSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }

    void D3D12Debug::ReportLiveD3D12Objects(ID3D12Device* d3d12Device)
    {
#if BENZIN_DEBUG_BUILD_ENABLED
        {
            ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
            BenzinD3D12Call(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
            BenzinD3D12Call(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false));
        }

        {
            ComPtr<ID3D12DebugDevice2> d3d12DebugDevice;
            BenzinD3D12Call(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));
            BenzinD3D12Call(d3d12DebugDevice->ReportLiveDeviceObjects(
                D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL));
        }
#endif
    }

    static void FormatToBuffer(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 d3d12DredAutoBreadcrumbsOutput, std::string& buffer)
    {
        std::format_to(std::back_inserter(buffer), "D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1\n");

        const D3D12_AUTO_BREADCRUMB_NODE1* d3d12AutoBreadcrumbNode = d3d12DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
        while (d3d12AutoBreadcrumbNode)
        {
            std::format_to(
                std::back_inserter(buffer),
                "  D3D12_AUTO_BREADCRUMB_NODE1: {}\n",
                (const void*)d3d12AutoBreadcrumbNode);

            if (d3d12AutoBreadcrumbNode->pCommandListDebugNameA)
            {
                std::format_to(
                    std::back_inserter(buffer),
                    "    D3D12 CommandList name: {}\n",
                    d3d12AutoBreadcrumbNode->pCommandListDebugNameA);
            }

            if (d3d12AutoBreadcrumbNode->pCommandQueueDebugNameA)
            {
                std::format_to(
                    std::back_inserter(buffer),
                    "    D3D12 CommandQueue name: {}\n",
                    d3d12AutoBreadcrumbNode->pCommandQueueDebugNameA);
            }

            if (d3d12AutoBreadcrumbNode->pLastBreadcrumbValue)
            {
                std::format_to(
                    std::back_inserter(buffer),
                    "    GPU-completed render operations: {}\n",
                    *d3d12AutoBreadcrumbNode->pLastBreadcrumbValue);
            }

            if (d3d12AutoBreadcrumbNode->BreadcrumbCount != 0)
            {
                std::format_to(
                    std::back_inserter(buffer),
                    "    Number of render operations used in the command list recording: {}:\n",
                    d3d12AutoBreadcrumbNode->BreadcrumbCount);

                const std::span<const D3D12_AUTO_BREADCRUMB_OP> d3d12AutoBreadcrumbOps{d3d12AutoBreadcrumbNode->pCommandHistory, d3d12AutoBreadcrumbNode->BreadcrumbCount};
                for (const auto [i, d3d12AutoBreadcrumbOp] : d3d12AutoBreadcrumbOps | std::views::enumerate)
                {
                    std::format_to(
                        std::back_inserter(buffer),
                        "    {}: {}\n",
                        i,
                        magic_enum::enum_name(d3d12AutoBreadcrumbOp));
                }
            }

            if (d3d12AutoBreadcrumbNode->BreadcrumbContextsCount != 0)
            {
                std::format_to(std::back_inserter(buffer), "    Breadcrumb Contexts:\n");

                const std::span<const D3D12_DRED_BREADCRUMB_CONTEXT> d3d12BreadcrumbContexts{d3d12AutoBreadcrumbNode->pBreadcrumbContexts, d3d12AutoBreadcrumbNode->BreadcrumbContextsCount};
                for (const auto& d3d12BreadcrumbContext : d3d12BreadcrumbContexts)
                {
                    std::format_to(
                        std::back_inserter(buffer),
                        "      BreadcrumbIndex: {}, Context: {}\n",
                        d3d12BreadcrumbContext.BreadcrumbIndex,
                        ToNarrowString(d3d12BreadcrumbContext.pContextString));
                }
            }

            d3d12AutoBreadcrumbNode = d3d12AutoBreadcrumbNode->pNext;
        }
    }

    static void FormatToBuffer(const D3D12_DRED_PAGE_FAULT_OUTPUT2& d3d12DredPageFaultOutput, std::string& buffer)
    {
        static const auto formatToBuffer = [](
            const D3D12_DRED_ALLOCATION_NODE1* d3d12DREDAllocationNode,
            std::string_view title,
            std::string& buffer)
        {
            if (d3d12DREDAllocationNode)
            {
                std::format_to(std::back_inserter(buffer), "{}\n", title);
            }

            while (d3d12DREDAllocationNode)
            {
                std::format_to(
                    std::back_inserter(buffer),
                    "  D3D12_DRED_ALLOCATION_NODE1: {}\n",
                    (const void*)d3d12DREDAllocationNode);

                if (d3d12DREDAllocationNode->ObjectNameA)
                {
                    std::format_to(
                        std::back_inserter(buffer),
                        "    D3D12 ObjectName: {}\n",
                        d3d12DREDAllocationNode->ObjectNameA);
                }

                std::format_to(
                    std::back_inserter(buffer),
                    "    AllocationType: {}\n",
                    magic_enum::enum_name(d3d12DREDAllocationNode->AllocationType));

                d3d12DREDAllocationNode = d3d12DREDAllocationNode->pNext;
            }
        };

        std::format_to(std::back_inserter(buffer), "D3D12_DRED_PAGE_FAULT_OUTPUT2\n");
        std::format_to(std::back_inserter(buffer), "PageFaultVA: {:#x}\n", d3d12DredPageFaultOutput.PageFaultVA);

        formatToBuffer(d3d12DredPageFaultOutput.pHeadExistingAllocationNode, "HeadExistingAllocationNode", buffer);
        formatToBuffer(d3d12DredPageFaultOutput.pHeadRecentFreedAllocationNode, "HeadRecentFreedAllocationNode", buffer);
    }

    std::string D3D12Debug::ProcessDredMessages(ID3D12Device* d3d12Device)
    {
        std::string buffer;
        buffer.reserve(1_mb);

        ComPtr<ID3D12DeviceRemovedExtendedData2> d3d12Dred;
        if (FAILED(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12Dred))))
        {
            BenzinError("Failed to query ID3D12DeviceRemovedExtendedData2 interface for DRED");
            return {};
        }

        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 d3d12DredAutoBreadcrumbsOutput = {};
        if (FAILED(d3d12Dred->GetAutoBreadcrumbsOutput1(&d3d12DredAutoBreadcrumbsOutput)))
        {
            BenzinError("Failed to get DRED AutoBreadcrumbs output");
            return {};
        }

        D3D12_DRED_PAGE_FAULT_OUTPUT2 d3d12DredPageFaultOutput = {};
        if (FAILED(d3d12Dred->GetPageFaultAllocationOutput2(&d3d12DredPageFaultOutput)))
        {
            BenzinError("Failed to get DRED PageFault output");
            return {};
        }

        const D3D12_DRED_DEVICE_STATE d3d12DredDeviceState = d3d12Dred->GetDeviceState();
        std::format_to(
            std::back_inserter(buffer),
            "D3D12_DRED_DEVICE_STATE: {}\n",
            magic_enum::enum_name(d3d12DredDeviceState));

        FormatToBuffer(d3d12DredAutoBreadcrumbsOutput, buffer);
        FormatToBuffer(d3d12DredPageFaultOutput, buffer);

        return buffer;
    }

}
