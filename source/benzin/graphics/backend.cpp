#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/backend.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/adl_wrapper.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/nvapi_wrapper.hpp"
#include "benzin/graphics/pix_capturer.hpp"

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
    {
        PixCapturer::Initialize(); // Need to be loaded first of all if needed

        AdlWrapper::Initialize();
        NvApiWrapper::Initialize();

        EnableD3D12DebugLayer();

        CreateDxgiFactory();
        GatherDxgiAdapters();

        const auto& mainAdapterInfo = GetMainAdapterInfo();
        BenzinTrace("----------------------------------------------");
        BenzinTrace("Main Adapter:");
        BenzinTrace("{}", m_AdaptersInfo[m_MainAdapterIndex].Name);
        BenzinTrace("DedicatedVideoMemory: {:.2f} mb, {:.2f} gb", mainAdapterInfo.TotalDedicatedVram.GetMb(), mainAdapterInfo.TotalDedicatedVram.GetGb());
        BenzinTrace("DedicatedSystemMemory: {:.2f} mb, {:.2f} gb", mainAdapterInfo.TotalDedicatedRam.GetMb(), mainAdapterInfo.TotalDedicatedRam.GetGb());
        BenzinTrace("SharedSystemMemory: {:.2f} mb, {:.2f} gb", mainAdapterInfo.TotalSharedRam.GetMb(), mainAdapterInfo.TotalSharedRam.GetGb());
        BenzinTrace("----------------------------------------------");
    }

    Backend::~Backend()
    {
        for (auto& dxgiAdapter : m_DxgiAdapters)
        {
            BenzinSafeDxObjectRelease(dxgiAdapter);
        }
        m_DxgiAdapters.clear();

        BenzinSafeDxObjectRelease(m_DxgiFactory);

        AdlWrapper::Shutdown();
        NvApiWrapper::Shutdown();

        PixCapturer::Shutdown();
    }

    const AdapterInfo& Backend::GetAdaptersInfo(uint32_t adapterIndex) const
    {
        BenzinAssert(adapterIndex < m_AdaptersInfo.size());
        return m_AdaptersInfo[adapterIndex];
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

        Bytes64 vendorTotalUsedDedicatedVram = g_InvalidUnsigned<uint64_t>;
        if (AdlWrapper::IsInitialized() && adapterInfo.IsAmd())
        {
            vendorTotalUsedDedicatedVram = AdlWrapper::GetUsedDedicatedVram(adapterInfo.DeviceId);
        }
        else if (NvApiWrapper::IsInitialized() && adapterInfo.IsNvidia())
        {
#if BENZIN_IS_ASSERTS_ENABLED
            const Bytes64 totalUsedDedicatedVram = NvApiWrapper::GetTotalDedicatedVram(adapterInfo.DeviceId);
            BenzinAssert(totalUsedDedicatedVram == adapterInfo.TotalDedicatedVram, "UsedDedicatedVram calculates relative to TotalDedicatedVram");
#endif

            vendorTotalUsedDedicatedVram = NvApiWrapper::GetUsedDedicatedVram(adapterInfo.DeviceId);
        }

        const Bytes64 dedicatedVramOsBudget = d3d12LocalVideoMemoryInfo.Budget;
        const bool isVendorDataValid = IsValidUnsigned(vendorTotalUsedDedicatedVram.GetBytes());

        return AdapterMemoryInfo
        {
            .DedicatedVramOsBudget = dedicatedVramOsBudget,
            .ProcessUsedDedicatedVram = d3d12LocalVideoMemoryInfo.CurrentUsage,
            .SharedRamOsBudget = d3d12NonLocalVideoMemoryInfo.Budget,
            .ProcessUsedSharedRam = d3d12NonLocalVideoMemoryInfo.CurrentUsage,
            .TotalUsedDedicatedVram = !isVendorDataValid ? 0_bytes64 : vendorTotalUsedDedicatedVram,
            .AvailableDedicatedVram = !isVendorDataValid ? 0_bytes64 : adapterInfo.TotalDedicatedVram - vendorTotalUsedDedicatedVram,
            .AvailableDedicatedVramRelativeToOsBudget = !isVendorDataValid ? 0_bytes64 : dedicatedVramOsBudget - vendorTotalUsedDedicatedVram,
        };
    }

    void Backend::CreateDxgiFactory()
    {
        const uint32_t dxgiFactoryFlags = BENZIN_IS_DEBUG_BUILD ? DXGI_CREATE_FACTORY_DEBUG : 0;

        ComPtr<IDXGIFactory2> dxgiFactory2;
        BenzinEnsure(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory2)));
        BenzinEnsure(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&m_DxgiFactory)));
        SetDxObjectDebugName(m_DxgiFactory, "MainFactory");
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

            AdapterInfo adapterInfo
            {
                .Name = ToNarrowString(dxgiAdapterDesc.Description),
                .VendorType = AdapterVendorIdToType(dxgiAdapterDesc.VendorId),
                .DeviceId = dxgiAdapterDesc.DeviceId,
                .TotalDedicatedVram = dxgiAdapterDesc.DedicatedVideoMemory,
                .TotalDedicatedRam = dxgiAdapterDesc.DedicatedSystemMemory,
                .TotalSharedRam = dxgiAdapterDesc.SharedSystemMemory,
            };

            BenzinTrace(
                "Adapter {}. {}, VendorId: {}, DeviceId: {}, SubSysId: {}, RevisionId: {}",
                adapterIndex,
                adapterInfo.Name,
                dxgiAdapterDesc.VendorId,
                dxgiAdapterDesc.DeviceId,
                dxgiAdapterDesc.SubSysId,
                dxgiAdapterDesc.Revision
            );

            if (IsStringContainsCaseInsensitive(adapterInfo.Name, CommandLineArgs::g_AdapterName))
            {
                m_MainAdapterIndex = adapterIndex;
            }

            IDXGIAdapter3* dxgiAdapter3 = nullptr;
            BenzinEnsure(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&dxgiAdapter3)));
            SetDxObjectDebugName(dxgiAdapter3, std::format("Adapter: {}", adapterInfo.Name));

            m_DxgiAdapters.push_back(dxgiAdapter3);
            m_AdaptersInfo.push_back(std::move(adapterInfo));
        }

        if (!IsValidUnsigned(m_MainAdapterIndex))
        {
            m_MainAdapterIndex = GetValidUnsignedOr(CommandLineArgs::g_AdapterIndex, 0u);
            BenzinEnsure(m_MainAdapterIndex < m_DxgiAdapters.size());
        }
    }

} // namespace benzin
