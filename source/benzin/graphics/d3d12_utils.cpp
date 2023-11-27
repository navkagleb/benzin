#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/d3d12_utils.hpp"

namespace benzin
{

    namespace
    {

        void FormatToBuffer(D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 d3d12DREDAutoBreadcrumbsOutput, std::string& buffer)
        {
            std::format_to(std::back_inserter(buffer), "D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1\n");

            const D3D12_AUTO_BREADCRUMB_NODE1* d3d12AutoBreadcrumbNode = d3d12DREDAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode;
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

        void FormatToBuffer(const D3D12_DRED_PAGE_FAULT_OUTPUT2& d3d12DREDPageFaultOutput, std::string& buffer)
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
            std::format_to(std::back_inserter(buffer), "PageFaultVA: {:#x}\n", d3d12DREDPageFaultOutput.PageFaultVA);

            FormatToBuffer(d3d12DREDPageFaultOutput.pHeadExistingAllocationNode, "HeadExistingAllocationNode", buffer);
            FormatToBuffer(d3d12DREDPageFaultOutput.pHeadRecentFreedAllocationNode, "HeadRecentFreedAllocationNode", buffer);
        }

    } // anonymous namespace

    void EnableD3D12DebugLayer(const DebugLayerParams& params)
    {
        ComPtr<ID3D12Debug5> d3d12Debug;
        BenzinAssert(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));

        d3d12Debug->EnableDebugLayer();
        d3d12Debug->SetEnableGPUBasedValidation(params.IsGPUBasedValidationEnabled);
        d3d12Debug->SetEnableSynchronizedCommandQueueValidation(params.IsSynchronizedCommandQueueValidationEnabled);
        d3d12Debug->SetEnableAutoName(true);

        BenzinTrace("{}", Logger::s_LineSeparator);
        BenzinTrace("D3D12DebugLayer enabled");
        BenzinTrace("GPUBasedValidation enabled: {}", params.IsGPUBasedValidationEnabled);
        BenzinTrace("SynchronizedCommandQueueValidation enabled: {}", params.IsSynchronizedCommandQueueValidationEnabled);
        BenzinTrace("AutoName enabled: true");
        BenzinTrace("{}", Logger::s_LineSeparator);
    }

    void EnableDRED()
    {
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> d3d12DREDSettings;
        BenzinAssert(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12DREDSettings)));

        d3d12DREDSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        d3d12DREDSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        d3d12DREDSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }

    void OnD3D12DeviceRemoved(PVOID context, BOOLEAN)
    {
        auto* d3d12Device = (ID3D12Device*)context;
        BenzinExecuteOnScopeExit([&d3d12Device] { d3d12Device->Release(); });

        BreakOnD3D12Error(d3d12Device, false);

        std::string buffer;
        buffer.reserve(2048);

        const HRESULT removedReason = d3d12Device->GetDeviceRemovedReason();
        switch (removedReason)
        {
            case DXGI_ERROR_DEVICE_HUNG: buffer += "DeviceRemovedReason: DXGI_ERROR_DEVICE_HUNG\n"; break;
            case DXGI_ERROR_DEVICE_REMOVED: buffer += "DeviceRemovedReason: DXGI_ERROR_DEVICE_REMOVED\n"; break;
            case DXGI_ERROR_DEVICE_RESET: buffer += "DeviceRemovedReason: DXGI_ERROR_DEVICE_RESET\n"; break;
            case DXGI_ERROR_DRIVER_INTERNAL_ERROR: buffer += "DeviceRemovedReason: DXGI_ERROR_DRIVER_INTERNAL_ERROR\n"; break;
            case DXGI_ERROR_INVALID_CALL: buffer += "DeviceRemovedReason: DXGI_ERROR_INVALID_CALL\n"; break;
            default: break;
        }

        ComPtr<ID3D12DeviceRemovedExtendedData2> d3d12DRED;
        BenzinAssert(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12DRED)));

        D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 d3d12DREDAutoBreadcrumbsOutput;
        BenzinAssert(d3d12DRED->GetAutoBreadcrumbsOutput1(&d3d12DREDAutoBreadcrumbsOutput));

        D3D12_DRED_PAGE_FAULT_OUTPUT2 d3d12DREDPageFaultOutput;
        BenzinAssert(d3d12DRED->GetPageFaultAllocationOutput2(&d3d12DREDPageFaultOutput));

        const D3D12_DRED_DEVICE_STATE d3d12DREDDeviceState = d3d12DRED->GetDeviceState();
        std::format_to(std::back_inserter(buffer), "D3D12_DRED_DEVICE_STATE: {}\n", magic_enum::enum_name(d3d12DREDDeviceState));

        FormatToBuffer(d3d12DREDAutoBreadcrumbsOutput, buffer);
        FormatToBuffer(d3d12DREDPageFaultOutput, buffer);

        BenzinError("\n{}", buffer);
        BenzinEnsure(false, "RemoveDevice was trigerred");
    }

    void BreakOnD3D12Error(ID3D12Device* d3d12Device, bool isBreak)
    {
        ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
        BenzinAssert(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12InfoQueue)));

        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, isBreak);
        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, isBreak);
        d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, isBreak);
    }

    void ReportLiveD3D12Objects(ID3D12Device* d3d12Device)
    {
        ComPtr<ID3D12DebugDevice2> d3d12DebugDevice;
        BenzinAssert(d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12DebugDevice)));

        d3d12DebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_DETAIL);
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

    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, const DebugName& debugName)
    {
        if (debugName.IsEmpty())
        {
            return;
        }

        if (!debugName.IsIndexValid())
        {
            SetD3D12ObjectDebugName(d3d12Object, debugName.Chars);
        }
        else
        {
            SetD3D12ObjectDebugName(d3d12Object, debugName.Chars, debugName.Index);
        }
    }

    void SetD3D12ObjectDebugName(ID3D12Object* d3d12Object, std::string_view debugName)
    {
        BenzinAssert(d3d12Object);

        if (debugName.empty())
        {
            return;
        }

        const bool isRegistered = !HasD3D12ObjectDebugName(d3d12Object);
        BenzinAssert(d3d12Object->SetPrivateData(WKPDID_D3DDebugObjectName, (uint32_t)debugName.size(), debugName.data()));

        if (isRegistered)
        {
            BenzinTrace("'{}' is registered", debugName);
        }
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
