#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/backend.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/graphics/adl_wrapper.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/nvapi_wrapper.hpp>
#include <benzin/graphics/pix_capturer.hpp>

// DirectX Agile SDK
// Ref: https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = BENZIN_AGILE_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = BENZIN_AGILE_SDK_PATH;
}

namespace benzin
{

    Backend::Backend()
    {
        PixCapturer::Initialize(); // Need to be loaded first of all if needed
        AdlWrapper::Initialize();
        NvApiWrapper::Initialize();

        EnableD3D12DebugLayer();
        EnableDred(); // TODO: Check if temp device is supported

        CreateDxgiFactory();
        GatherDxgiAdapters();

        const AdapterInfo& mainAdapterInfo = GetMainAdapterInfo();
        BenzinTrace("Selected Adapter:");
        BenzinTrace("  {}", m_AdaptersInfo[m_MainAdapterIndex].m_Name);
        BenzinTrace("  Local VRAM: {:.2f} mb, {:.2f} gb", ToMb(mainAdapterInfo.m_TotalLocalVramInBytes), ToGb(mainAdapterInfo.m_TotalLocalVramInBytes));
        BenzinTrace("  Host VRAM: {:.2f} mb, {:.2f} gb", ToMb(mainAdapterInfo.m_TotalHostVramInBytes), ToGb(mainAdapterInfo.m_TotalHostVramInBytes));

        if (!IsMaxUint(mainAdapterInfo.m_GpuCoreCount))
        {
            BenzinTrace("  GPU Core Count: {}", AdlWrapper::GetGpuCoreCount());
        }
    }

    Backend::~Backend()
    {
        BenzinTraceScopeTime("Backend::~Backend");

#if BENZIN_IS_ASSERTS_ENABLED
        if (!CmdLineArgs::IsPixCapturerEnabled())
        {
            const AdapterMemoryInfo adapterMemoryInfo = GetMainAdapterMemoryInfo();
            BenzinAssert(adapterMemoryInfo.m_UsedLocalVramInBytes == 0, "Used Local VRAM: {} mb", ToMb(adapterMemoryInfo.m_UsedLocalVramInBytes));
            BenzinAssert(adapterMemoryInfo.m_UsedHostVramInBytes == 0, "Used Host VRAM: {} mb", ToMb(adapterMemoryInfo.m_UsedHostVramInBytes));
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

        const AdapterInfo& adapterInfo = m_AdaptersInfo[adapterIndex];
        IDXGIAdapter3* dxgiAdapter = m_DxgiAdapters[adapterIndex];

        DXGI_QUERY_VIDEO_MEMORY_INFO d3d12LocalVideoMemoryInfo;
        BenzinD3D12Call(dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &d3d12LocalVideoMemoryInfo));

        DXGI_QUERY_VIDEO_MEMORY_INFO d3d12NonLocalVideoMemoryInfo;
        BenzinD3D12Call(dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &d3d12NonLocalVideoMemoryInfo));

        uint64_t vendorTotalUsedVramInBytes = g_MaxU64;
        if (AdlWrapper::IsAvailable() && adapterInfo.IsAmd())
        {
            vendorTotalUsedVramInBytes = AdlWrapper::GetUsedDedicatedVramInBytes();
        }
        else if (NvApiWrapper::IsAvailable() && adapterInfo.IsNvidia())
        {
#if BENZIN_IS_ASSERTS_ENABLED
            const uint64_t totalVramInBytes = NvApiWrapper::GetTotalDedicatedVramInBytes();
            BenzinAssert(
                totalVramInBytes == adapterInfo.m_TotalLocalVramInBytes,\
                "DXGI Total VRAM don't equal to NvAPI Total VRAM! DXGI VRAM: {}, NvAPI VRAM: {}",
                ToMb(adapterInfo.m_TotalLocalVramInBytes),
                ToMb(totalVramInBytes));
#endif

            vendorTotalUsedVramInBytes = NvApiWrapper::GetUsedDedicatedVramInBytes();
        }

        const uint64_t localVramBudgetInBytes = d3d12LocalVideoMemoryInfo.Budget;
        const bool isVendorDataValid = !IsMaxUint(localVramBudgetInBytes);

        return AdapterMemoryInfo
        {
            .m_LocalVramBudgetInBytes = localVramBudgetInBytes,
            .m_UsedLocalVramInBytes = d3d12LocalVideoMemoryInfo.CurrentUsage,
            .m_HostVramBudgetInBytes = d3d12NonLocalVideoMemoryInfo.Budget,
            .m_UsedHostVramInBytes = d3d12NonLocalVideoMemoryInfo.CurrentUsage,
            .m_TotalUsedVramInBytes = isVendorDataValid ? vendorTotalUsedVramInBytes : 0,
            .m_AvailableVramInBytes = isVendorDataValid ? adapterInfo.m_TotalLocalVramInBytes - vendorTotalUsedVramInBytes : 0,
            .m_AvailableVramRelativeToOsBudgetInBytes =
                isVendorDataValid && localVramBudgetInBytes > vendorTotalUsedVramInBytes ?
                localVramBudgetInBytes - vendorTotalUsedVramInBytes :
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
                break;

            DXGI_ADAPTER_DESC1 dxgiAdapterDesc{};
            BenzinD3D12Call(dxgiAdapter->GetDesc1(&dxgiAdapterDesc));

            if ((dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 || dxgiAdapterDesc.DedicatedVideoMemory == 0)
                continue;

            AdapterInfo adapterInfo
            {
                .m_Name = ToNarrowString(dxgiAdapterDesc.Description),
                .m_VendorId = dxgiAdapterDesc.VendorId,
                .m_DeviceId = dxgiAdapterDesc.DeviceId,
                .m_TotalLocalVramInBytes = dxgiAdapterDesc.DedicatedVideoMemory,
                .m_TotalHostVramInBytes = dxgiAdapterDesc.SharedSystemMemory,
            };

            if (AdlWrapper::IsAvailable() && adapterInfo.IsAmd())
            {
                adapterInfo.m_GpuCoreCount = AdlWrapper::GetGpuCoreCount();
            }
            else if (NvApiWrapper::IsAvailable() && adapterInfo.IsNvidia())
            {
                adapterInfo.m_GpuCoreCount = NvApiWrapper::GetGpuCoreCount();
            }

            BenzinTrace(
                "Adapter {}. {}, VendorId: {}, DeviceId: {}, SubSysId: {}, RevisionId: {}",
                adapterIndex,
                adapterInfo.m_Name,
                dxgiAdapterDesc.VendorId,
                dxgiAdapterDesc.DeviceId,
                dxgiAdapterDesc.SubSysId,
                dxgiAdapterDesc.Revision
            );

            if (IsStringContainsCaseInsensitive(adapterInfo.m_Name, CmdLineArgs::GetAdapterName()))
            {
                m_MainAdapterIndex = adapterIndex;
            }

            IDXGIAdapter3* dxgiAdapter3 = nullptr;
            BenzinD3D12Call(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&dxgiAdapter3)));
            SetD3DObjectDebugName(dxgiAdapter3, std::format("Adapter: {}", adapterInfo.m_Name));

            m_DxgiAdapters.push_back(dxgiAdapter3);
            m_AdaptersInfo.push_back(std::move(adapterInfo));
        }

        if (IsMaxUint(m_MainAdapterIndex))
        {
            m_MainAdapterIndex = !IsMaxUint(CmdLineArgs::GetAdapterIndex()) ? CmdLineArgs::GetAdapterIndex() : 0;
            BenzinEnsure(m_MainAdapterIndex < m_DxgiAdapters.size());
        }
    }

}
