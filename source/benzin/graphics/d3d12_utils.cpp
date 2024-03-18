#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/d3d12_utils.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/core/command_line_args.hpp"

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
                    std::format_to(std::back_inserter(buffer), "      BreadcrumbIndex: {}, Context: {}", d3d12BreadcrumbContext.BreadcrumbIndex, ToNarrowString(d3d12BreadcrumbContext.pContextString));
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

#if BENZIN_IS_DEBUG_BUILD

    void EnableD3D12DebugLayer()
    {
        // Note: Enabling the debug layer after device creation will invalidate the active device

        ComPtr<ID3D12Debug5> d3d12Debug;
        BenzinAssert(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));

        d3d12Debug->EnableDebugLayer();

        const auto& params = CommandLineArgs::GetGraphicsDebugLayerParams();
        d3d12Debug->SetEnableGPUBasedValidation(params.IsGpuBasedValidationEnabled);
        d3d12Debug->SetEnableSynchronizedCommandQueueValidation(params.IsSynchronizedCommandQueueValidationEnabled);
        d3d12Debug->SetEnableAutoName(true);

        BenzinTrace("----------------------------------------------");
        BenzinTrace("D3D12DebugLayer enabled");
        BenzinTrace("GPUBasedValidation enabled: {}", params.IsGpuBasedValidationEnabled);
        BenzinTrace("SynchronizedCommandQueueValidation enabled: {}", params.IsSynchronizedCommandQueueValidationEnabled);
        BenzinTrace("AutoName enabled: true");
        BenzinTrace("----------------------------------------------");
    }

    void EnableD3D12DebugBreakOn(ID3D12Device* d3d12Device, bool isEnabled, D3D12BreakReasonFlags flags)
    {
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        BenzinAssert(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue)));
        
        ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
        BenzinAssert(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));
        
        if (flags[D3D12BreakReasonFlag::Warning])
        {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, isEnabled);
            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, isEnabled);
        }
        
        if (flags[D3D12BreakReasonFlag::Error])
        {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, isEnabled);
            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, isEnabled);
        }
        
        if (flags[D3D12BreakReasonFlag::Corruption])
        {
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, isEnabled);
            d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, isEnabled);
        }
    }

    void ReportLiveD3D12Objects(ID3D12Device* d3d12Device)
    {
        ComPtr<ID3D12DebugDevice2> d3d12DebugDevice;
        BenzinAssert(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));

        d3d12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL);
    }

#endif // BENZIN_IS_DEBUG_BUILD

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
        BenzinAssert(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12DredSettings)));

        d3d12DredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        d3d12DredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        d3d12DredSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }

    std::string GetDredMessages(ID3D12Device* d3d12Device)
    {
        std::string buffer;
        buffer.reserve(MbToBytes(1));

        ComPtr<ID3D12DeviceRemovedExtendedData2> d3d12Dred;
        BenzinAssert(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12Dred)));

        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 d3d12DredAutoBreadcrumbsOutput;
        BenzinAssert(d3d12Dred->GetAutoBreadcrumbsOutput1(&d3d12DredAutoBreadcrumbsOutput));

        D3D12_DRED_PAGE_FAULT_OUTPUT2 d3d12DredPageFaultOutput;
        BenzinAssert(d3d12Dred->GetPageFaultAllocationOutput2(&d3d12DredPageFaultOutput));

        const D3D12_DRED_DEVICE_STATE d3d12DredDeviceState = d3d12Dred->GetDeviceState();
        std::format_to(std::back_inserter(buffer), "D3D12_DRED_DEVICE_STATE: {}\n", magic_enum::enum_name(d3d12DredDeviceState));

        FormatToBuffer(d3d12DredAutoBreadcrumbsOutput, buffer);
        FormatToBuffer(d3d12DredPageFaultOutput, buffer);

        return buffer;
    }

    bool HasD3D12ObjectDebugName(ID3D12Object* d3d12Object)
    {
        BenzinAssert(d3d12Object);

        uint32_t bufferSize = 0;
        return SUCCEEDED(d3d12Object->GetPrivateData(WKPDID_D3DDebugObjectName, &bufferSize, nullptr));
    }

    std::string GetD3D12ObjectDebugName(ID3D12Object* d3d12Object)
    {
        static const size_t maxDebugNameSize = 128;

        BenzinAssert(d3d12Object);

        std::string debugName;
        debugName.resize_and_overwrite(maxDebugNameSize, [&](char* data, size_t size)
        {
            auto alignedSize = (uint32_t)size;

            BenzinAssert(d3d12Object->GetPrivateData(WKPDID_D3DDebugObjectName, &alignedSize, data));
            return alignedSize;
        });

        return debugName;
    }

    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName)
    {
        if (debugName.empty())
        {
            return;
        }

        BenzinAssert(d3d12Object);
        BenzinAssert(d3d12Object->SetPrivateData(WKPDID_D3DDebugObjectName, (uint32_t)debugName.size(), debugName.data()));
    }

    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName, uint32_t index)
    {
        SetD3D12ObjectDebugName(d3d12Object, std::format("{}{}", debugName, index));
    }

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

} // namespace benzin
