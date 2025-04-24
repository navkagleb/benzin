#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/backend.hpp"

#include "benzin/core/cmd_line_args.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/adl_wrapper.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/d3d12_assert.hpp"
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
        BenzinTrace(Logger::GetLineSeparator());
        BenzinTrace("Main Adapter:");
        BenzinTrace("{}", m_AdaptersInfo[m_MainAdapterIndex].Name);
        BenzinTrace("VRAM: {:.2f} mb, {:.2f} gb", ToMb(mainAdapterInfo.TotalVramInBytes), ToGb(mainAdapterInfo.TotalVramInBytes));
        BenzinTrace("RAM: {:.2f} mb, {:.2f} gb", ToMb(mainAdapterInfo.TotalRamInBytes), ToGb(mainAdapterInfo.TotalRamInBytes));
        BenzinTrace("Shared RAM: {:.2f} mb, {:.2f} gb", ToMb(mainAdapterInfo.TotalSharedRamInBytes), ToGb(mainAdapterInfo.TotalSharedRamInBytes));
        BenzinTrace(Logger::GetLineSeparator());
    }

    Backend::~Backend()
    {
        BenzinLogTimeOnScopeExit("Backend::~Backend");

#if BENZIN_IS_ASSERTS_ENABLED
        if (!CmdLineArgs::IsPixCapturerEnabled())
        {
            const auto adapterMemoryInfo = GetMainAdapterMemoryInfo();
            BenzinAssert(adapterMemoryInfo.ProcessUsedVramInBytes == 0, "Process used VRAM: {} mb", ToMb(adapterMemoryInfo.ProcessUsedVramInBytes));
            BenzinAssert(adapterMemoryInfo.ProcessUsedSharedRamInBytes == 0, "Process used Shared Ram: {} mb", ToMb(adapterMemoryInfo.ProcessUsedSharedRamInBytes));
        }
#endif

        NvApiWrapper::Shutdown();
        AdlWrapper::Shutdown();
        PixCapturer::Shutdown();

        for (auto& dxgiAdapter : m_DxgiAdapters)
        {
            SafeReleaseD3DObject(dxgiAdapter);
        }
        m_DxgiAdapters.clear();

        SafeReleaseD3DObject(m_DxgiFactory);
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
        BenzinD3D12Call(dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &d3d12LocalVideoMemoryInfo));

        DXGI_QUERY_VIDEO_MEMORY_INFO d3d12NonLocalVideoMemoryInfo;
        BenzinD3D12Call(dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &d3d12NonLocalVideoMemoryInfo));

        uint64_t vendorTotalUsedVramInBytes = g_Bad64;
        if (AdlWrapper::IsAvailable() && adapterInfo.IsAmd())
        {
            vendorTotalUsedVramInBytes = AdlWrapper::GetUsedDedicatedVramInBytes(adapterInfo.DeviceId);
        }
        else if (NvApiWrapper::IsAvailable() && adapterInfo.IsNvidia())
        {
#if BENZIN_IS_ASSERTS_ENABLED
            const uint64_t totalVramInBytes = NvApiWrapper::GetTotalDedicatedVramInBytes(adapterInfo.DeviceId);
            BenzinAssert(
                totalVramInBytes == adapterInfo.TotalVramInBytes,\
                "DXGI Total VRAM don't equal to NvAPI Total VRAM! DXGI VRAM: {}, NvAPI VRAM: {}",
                ToMb(adapterInfo.TotalVramInBytes),
                ToMb(totalVramInBytes)
            );
#endif

            vendorTotalUsedVramInBytes = NvApiWrapper::GetUsedDedicatedVramInBytes(adapterInfo.DeviceId);
        }

        const uint64_t vramOsBudgetInBytes = d3d12LocalVideoMemoryInfo.Budget;
        const bool isVendorDataValid = IsGoodUint(vramOsBudgetInBytes);

        return AdapterMemoryInfo
        {
            .VramOsBudgetInBytes = vramOsBudgetInBytes,
            .ProcessUsedVramInBytes = d3d12LocalVideoMemoryInfo.CurrentUsage,
            .SharedRamOsBudgetInBytes = d3d12NonLocalVideoMemoryInfo.Budget,
            .ProcessUsedSharedRamInBytes = d3d12NonLocalVideoMemoryInfo.CurrentUsage,
            .TotalUsedVramInBytes = isVendorDataValid ? vendorTotalUsedVramInBytes : 0,
            .AvailableVramInBytes = isVendorDataValid ? adapterInfo.TotalVramInBytes - vendorTotalUsedVramInBytes : 0,
            .AvailableVramRelativeToOsBudgetInBytes =
                isVendorDataValid && vramOsBudgetInBytes > vendorTotalUsedVramInBytes ?
                vramOsBudgetInBytes - vendorTotalUsedVramInBytes :
                0,
        };
    }

    void Backend::CreateDxgiFactory()
    {
        const uint32_t dxgiFactoryFlags = BENZIN_IS_DEBUG_BUILD ? DXGI_CREATE_FACTORY_DEBUG : 0;

        ComPtr<IDXGIFactory2> dxgiFactory2;
        BenzinD3D12Call(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory2)));
        BenzinD3D12Call(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&m_DxgiFactory)));
        SetD3DObjectDebugName(m_DxgiFactory, "MainFactory");
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
            BenzinD3D12Call(dxgiAdapter->GetDesc1(&dxgiAdapterDesc));

            if ((dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 || dxgiAdapterDesc.DedicatedVideoMemory == 0)
            {
                continue;
            }

            AdapterInfo adapterInfo
            {
                .Name = ToNarrowString(dxgiAdapterDesc.Description),
                .VendorType = AdapterVendorIdToType(dxgiAdapterDesc.VendorId),
                .DeviceId = dxgiAdapterDesc.DeviceId,
                .TotalVramInBytes = dxgiAdapterDesc.DedicatedVideoMemory,
                .TotalRamInBytes = dxgiAdapterDesc.DedicatedSystemMemory,
                .TotalSharedRamInBytes = dxgiAdapterDesc.SharedSystemMemory,
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

            if (IsStringContainsCaseInsensitive(adapterInfo.Name, CmdLineArgs::GetAdapterName()))
            {
                m_MainAdapterIndex = adapterIndex;
            }

            IDXGIAdapter3* dxgiAdapter3 = nullptr;
            BenzinD3D12Call(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&dxgiAdapter3)));
            SetD3DObjectDebugName(dxgiAdapter3, std::format("Adapter: {}", adapterInfo.Name));

            m_DxgiAdapters.push_back(dxgiAdapter3);
            m_AdaptersInfo.push_back(std::move(adapterInfo));
        }

        if (!IsGoodUint(m_MainAdapterIndex))
        {
            m_MainAdapterIndex = GetGoodUintOr(CmdLineArgs::GetAdapterIndex(), 0u);
            BenzinEnsure(m_MainAdapterIndex < m_DxgiAdapters.size());
        }
    }

}
