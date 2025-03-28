#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/backend.hpp"

#include "benzin/core/command_line_args.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/adl_wrapper.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/hr_assert.hpp"
#include "benzin/graphics/nvapi_wrapper.hpp"
#include "benzin/graphics/pix_capturer.hpp"

// DirectX Agile SDK
// Ref: https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = BENZIN_AGILE_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = BENZIN_AGILE_SDK_PATH;
}

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
        EnableDred(); // TODO: Check if temp device is supported

        CreateDxgiFactory();
        GatherDxgiAdapters();

        const auto& mainAdapterInfo = GetMainAdapterInfo();
        BenzinTrace(Logger::s_LineSeparator);
        BenzinTrace("Main Adapter:");
        BenzinTrace("{}", m_AdaptersInfo[m_MainAdapterIndex].Name);
        BenzinTrace("VRAM: {:.2f} mb, {:.2f} gb", mainAdapterInfo.TotalVram.GetMb(), mainAdapterInfo.TotalVram.GetGb());
        BenzinTrace("RAM: {:.2f} mb, {:.2f} gb", mainAdapterInfo.TotalRam.GetMb(), mainAdapterInfo.TotalRam.GetGb());
        BenzinTrace("Shared RAM: {:.2f} mb, {:.2f} gb", mainAdapterInfo.TotalSharedRam.GetMb(), mainAdapterInfo.TotalSharedRam.GetGb());
        BenzinTrace(Logger::s_LineSeparator);
    }

    Backend::~Backend()
    {
        BenzinLogTimeOnScopeExit("Backend::~Backend");

#if BENZIN_IS_ASSERTS_ENABLED
        if (!CommandLineArgs::GetBool("IsPixCapturerEnabled"))
        {
            const auto adapterMemoryInfo = GetMainAdapterMemoryInfo();
            BenzinAssert(adapterMemoryInfo.ProcessUsedVram == 0, "Process used VRAM: {} mb", adapterMemoryInfo.ProcessUsedVram.GetMb());
            BenzinAssert(adapterMemoryInfo.ProcessUsedSharedRam == 0, "Process used Shared Ram: {} mb", adapterMemoryInfo.ProcessUsedSharedRam.GetMb());
        }
#endif

        NvApiWrapper::Shutdown();
        AdlWrapper::Shutdown();
        PixCapturer::Shutdown();

        for (auto& dxgiAdapter : m_DxgiAdapters)
        {
            BenzinSafeDxObjectRelease(dxgiAdapter);
        }
        m_DxgiAdapters.clear();

        BenzinSafeDxObjectRelease(m_DxgiFactory);
    }

    const AdapterInfo& Backend::GetAdapterInfo(uint32_t adapterIndex) const
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
        BenzinHrEnsure(dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &d3d12LocalVideoMemoryInfo));

        DXGI_QUERY_VIDEO_MEMORY_INFO d3d12NonLocalVideoMemoryInfo;
        BenzinHrEnsure(dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &d3d12NonLocalVideoMemoryInfo));

        Bytes64 vendorTotalUsedVram = g_InvalidUnsigned<uint64_t>;
        if (AdlWrapper::IsAvailable() && adapterInfo.IsAmd())
        {
            vendorTotalUsedVram = AdlWrapper::GetUsedDedicatedVram(adapterInfo.DeviceId);
        }
        else if (NvApiWrapper::IsAvailable() && adapterInfo.IsNvidia())
        {
#if BENZIN_IS_ASSERTS_ENABLED
            const Bytes64 totalVram = NvApiWrapper::GetTotalDedicatedVram(adapterInfo.DeviceId);
            BenzinAssert(
                totalVram == adapterInfo.TotalVram,\
                "DXGI Total VRAM don't equal to NvAPI Total VRAM! DXGI VRAM: {}, NvAPI VRAM: {}",
                adapterInfo.TotalVram.GetMb(),
                totalVram.GetMb()
            );
#endif

            vendorTotalUsedVram = NvApiWrapper::GetUsedDedicatedVram(adapterInfo.DeviceId);
        }

        const Bytes64 vramOsBudget = d3d12LocalVideoMemoryInfo.Budget;
        const bool isVendorDataValid = IsValidUnsigned(vendorTotalUsedVram.GetByteCount());

        return AdapterMemoryInfo
        {
            .VramOsBudget = vramOsBudget,
            .ProcessUsedVram = d3d12LocalVideoMemoryInfo.CurrentUsage,
            .SharedRamOsBudget = d3d12NonLocalVideoMemoryInfo.Budget,
            .ProcessUsedSharedRam = d3d12NonLocalVideoMemoryInfo.CurrentUsage,
            .TotalUsedVram = isVendorDataValid ? vendorTotalUsedVram : 0_bytes64,
            .AvailableVram = isVendorDataValid ? adapterInfo.TotalVram - vendorTotalUsedVram : 0_bytes64,
            .AvailableVramRelativeToOsBudget =
                isVendorDataValid && vramOsBudget > vendorTotalUsedVram ?
                vramOsBudget - vendorTotalUsedVram :
                0_bytes64,
        };
    }

    void Backend::CreateDxgiFactory()
    {
        const uint32_t dxgiFactoryFlags = BENZIN_IS_DEBUG_BUILD ? DXGI_CREATE_FACTORY_DEBUG : 0;

        ComPtr<IDXGIFactory2> dxgiFactory2;
        BenzinHrEnsure(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory2)));
        BenzinHrEnsure(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&m_DxgiFactory)));
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
            BenzinHrEnsure(dxgiAdapter->GetDesc1(&dxgiAdapterDesc));

            if ((dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 || dxgiAdapterDesc.DedicatedVideoMemory == 0)
            {
                continue;
            }

            AdapterInfo adapterInfo
            {
                .Name = ToNarrowString(dxgiAdapterDesc.Description),
                .VendorType = AdapterVendorIdToType(dxgiAdapterDesc.VendorId),
                .DeviceId = dxgiAdapterDesc.DeviceId,
                .TotalVram = dxgiAdapterDesc.DedicatedVideoMemory,
                .TotalRam = dxgiAdapterDesc.DedicatedSystemMemory,
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

            if (IsStringContainsCaseInsensitive(adapterInfo.Name, CommandLineArgs::GetString("AdapterName")))
            {
                m_MainAdapterIndex = adapterIndex;
            }

            IDXGIAdapter3* dxgiAdapter3 = nullptr;
            BenzinHrEnsure(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&dxgiAdapter3)));
            SetDxObjectDebugName(dxgiAdapter3, std::format("Adapter: {}", adapterInfo.Name));

            m_DxgiAdapters.push_back(dxgiAdapter3);
            m_AdaptersInfo.push_back(std::move(adapterInfo));
        }

        if (!IsValidUnsigned(m_MainAdapterIndex))
        {
            m_MainAdapterIndex = GetValidUnsignedOr(CommandLineArgs::GetU32("AdapterIndex"), 0u);
            BenzinEnsure(m_MainAdapterIndex < m_DxgiAdapters.size());
        }
    }

}
