#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/backend.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/adl_wrapper.hpp"
#include "benzin/graphics/nvapi_wrapper.hpp"

namespace benzin
{

    static AdapterVendorType AdapterVendorIdToType(uint32_t vendorId)
    {
        switch (vendorId)
        {
            case 0x1002:
            case 0x1022: return AdapterVendorType::Amd;
            case 0x10DE: return AdapterVendorType::Nvidia;
        }

        return AdapterVendorType::Other;
    }

    Backend::Backend()
        : m_MainAdapterIndex{ CommandLineArgs::GetAdapterIndex() }
    {
#if BENZIN_IS_DEBUG_BUILD
        EnableD3D12DebugLayer();
#endif

        AdlWrapper::Initialize();
        NvApiWrapper::Initialize();

        CreateDxgiFactory();
        GatherDxgiAdapters();
        QueryDxgiMainAdapter();
    }

    Backend::~Backend()
    {
        AdlWrapper::Shutdown();
        NvApiWrapper::Shutdown();

        for (auto& dxgiAdapter : m_DxgiAdapters)
        {
            SafeUnknownRelease(dxgiAdapter);
        }

        SafeUnknownRelease(m_MainDxgiAdapter);
        SafeUnknownRelease(m_DxgiFactory);
    }

    const AdapterInfo& Backend::GetAdaptersInfo(uint32_t adapterIndex) const
    {
        BenzinAssert(adapterIndex < m_AdaptersInfo.size());
        return m_AdaptersInfo[adapterIndex];
    }

    const AdapterInfo& Backend::GetMainAdapterInfo() const
    {
        return GetAdaptersInfo(m_MainAdapterIndex);
    }

    AdapterMemoryInfo Backend::GetAdapterMemoryInfo(uint32_t adapterIndex) const
    {
        BenzinAssert(adapterIndex < m_DxgiAdapters.size());

        const auto& adapterInfo = m_AdaptersInfo[adapterIndex];
        auto* dxgiAdapter = m_DxgiAdapters[adapterIndex];

        DXGI_QUERY_VIDEO_MEMORY_INFO d3d12LocalVideoMemoryInfo;
        BenzinEnsure(dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &d3d12LocalVideoMemoryInfo));

        DXGI_QUERY_VIDEO_MEMORY_INFO d3d12NonLocalVideoMemoryInfo;
        BenzinEnsure(dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &d3d12NonLocalVideoMemoryInfo));

        const uint64_t dedicatedVramOsBudget = d3d12LocalVideoMemoryInfo.Budget;

        uint64_t vendorTotalUsedDedicatedVramInBytes = g_InvalidIndex<uint64_t>;
        if (adapterInfo.IsAmd())
        {
            vendorTotalUsedDedicatedVramInBytes = AdlWrapper::GetUsedDedicatedVramInBytes(adapterInfo.PciIdentifiers);
        }
        else if (adapterInfo.IsNvidia())
        {
            const uint64_t totalUsedDedicatedVram = NvApiWrapper::GetTotalDedicatedVramInBytes(adapterInfo.PciIdentifiers);
            BenzinAssert(totalUsedDedicatedVram == adapterInfo.TotalDedicatedVramInBytes, "UsedDedicatedVram calculates relative to TotalDedicatedVram");

            vendorTotalUsedDedicatedVramInBytes = NvApiWrapper::GetUsedDedicatedVramInBytes(adapterInfo.PciIdentifiers);
        }

        const bool isVendorDataValid = vendorTotalUsedDedicatedVramInBytes != g_InvalidIndex<uint64_t>;
        return AdapterMemoryInfo
        {
            .DedicatedVramOsBudgetInBytes = dedicatedVramOsBudget,
            .ProcessUsedDedicatedVramInBytes = d3d12LocalVideoMemoryInfo.CurrentUsage,
            .SharedRamOsBudgetInBytes = d3d12NonLocalVideoMemoryInfo.Budget,
            .ProcessUsedSharedRamInBytes = d3d12NonLocalVideoMemoryInfo.CurrentUsage,
            .TotalUsedDedicatedVramInBytes = !isVendorDataValid ? 0 : vendorTotalUsedDedicatedVramInBytes,
            .AvailableDedicatedVramInBytes = !isVendorDataValid ? 0 : adapterInfo.TotalDedicatedVramInBytes - vendorTotalUsedDedicatedVramInBytes,
            .AvailableDedicatedVramRelativeToOsBudgetInBytes = !isVendorDataValid ? 0 : dedicatedVramOsBudget - vendorTotalUsedDedicatedVramInBytes,
        };
    }

    AdapterMemoryInfo Backend::GetMainAdapterMemoryInfo() const
    {
        return GetAdapterMemoryInfo(m_MainAdapterIndex);
    }

    void Backend::CreateDxgiFactory()
    {
        const uint32_t dxgiFactoryFlags = BENZIN_IS_DEBUG_BUILD ? DXGI_CREATE_FACTORY_DEBUG : 0;

        ComPtr<IDXGIFactory2> dxgiFactory2;
        BenzinEnsure(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory2)));
        BenzinEnsure(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&m_DxgiFactory)));
    }

    void Backend::GatherDxgiAdapters()
    {
        BenzinTrace("DXGI Available adapters:");

        for (uint32_t adapterIndex = 0; true; ++adapterIndex)
        {
            ComPtr<IDXGIAdapter1> dxgiAdapter;
            if (FAILED(m_DxgiFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&dxgiAdapter))))
            {
                break;
            }

            DXGI_ADAPTER_DESC1 dxgiAdapterDesc{};
            BenzinEnsure(dxgiAdapter->GetDesc1(&dxgiAdapterDesc));

            if ((dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 || dxgiAdapterDesc.DedicatedVideoMemory == 0)
            {
                continue;
            }

            const AdapterInfo adapterInfo
            {
                .Name = ToNarrowString(dxgiAdapterDesc.Description),
                .VendorType = AdapterVendorIdToType(dxgiAdapterDesc.VendorId),
                .PciIdentifiers
                {
                    .VendorId = dxgiAdapterDesc.VendorId,
                    .DeviceId = dxgiAdapterDesc.DeviceId,
                    .SubSysId = dxgiAdapterDesc.SubSysId,
                    .RevisionId = dxgiAdapterDesc.Revision,
                },
                .TotalDedicatedVramInBytes = dxgiAdapterDesc.DedicatedVideoMemory,
                .TotalDedicatedRamInBytes = dxgiAdapterDesc.DedicatedSystemMemory,
                .TotalSharedRamInBytes = dxgiAdapterDesc.SharedSystemMemory,
            };

            BenzinTrace(
                "Adapter {}. {}, VendorId: {}, DeviceId: {}, SubSysId: {}, RevisionId: {}",
                adapterIndex,
                adapterInfo.Name,
                adapterInfo.PciIdentifiers.VendorId,
                adapterInfo.PciIdentifiers.DeviceId,
                adapterInfo.PciIdentifiers.SubSysId,
                adapterInfo.PciIdentifiers.RevisionId
            );

            BenzinEnsure(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&m_MainDxgiAdapter)));

            IDXGIAdapter3* dxgiAdapter3 = nullptr;
            BenzinEnsure(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&dxgiAdapter3)));

            m_DxgiAdapters.push_back(dxgiAdapter3);
            m_AdaptersInfo.push_back(adapterInfo);
        }
    }

    void Backend::QueryDxgiMainAdapter()
    {
        BenzinAssert(m_MainAdapterIndex < m_DxgiAdapters.size());

        IDXGIAdapter* dxgiAdapter = m_DxgiAdapters[m_MainAdapterIndex];
        BenzinEnsure(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&m_MainDxgiAdapter)));

        const auto& mainAdapterInfo = m_AdaptersInfo[m_MainAdapterIndex];
        BenzinTrace("----------------------------------------------");
        BenzinTrace("Main Adapter:");
        BenzinTrace("{}", m_AdaptersInfo[m_MainAdapterIndex].Name);
        BenzinTrace("DedicatedVideoMemory: {:.2f} mb, {:.2f} gb", BytesToFloatMb(mainAdapterInfo.TotalDedicatedVramInBytes), BytesToFloatGb(mainAdapterInfo.TotalDedicatedVramInBytes));
        BenzinTrace("DedicatedSystemMemory: {:.2f} mb, {:.2f} gb", BytesToFloatMb(mainAdapterInfo.TotalDedicatedRamInBytes), BytesToFloatGb(mainAdapterInfo.TotalDedicatedRamInBytes));
        BenzinTrace("SharedSystemMemory: {:.2f} mb, {:.2f} gb", BytesToFloatMb(mainAdapterInfo.TotalSharedRamInBytes), BytesToFloatGb(mainAdapterInfo.TotalSharedRamInBytes));
        BenzinTrace("----------------------------------------------");
    }

} // namespace benzin

size_t std::hash<benzin::PciIdentifiers>::operator()(const benzin::PciIdentifiers& pciIdentifiers) const
{
    size_t result = 0;
    result = benzin::HashCombine(result, pciIdentifiers.VendorId);
    result = benzin::HashCombine(result, pciIdentifiers.DeviceId);
    result = benzin::HashCombine(result, pciIdentifiers.SubSysId);
    result = benzin::HashCombine(result, pciIdentifiers.RevisionId);

    return result;
}
