#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/backend.hpp"

#include "benzin/core/common.hpp"
#include "benzin/graphics/api/common.hpp"
#include "benzin/utility/string_utils.h"

namespace benzin
{

    namespace
    {

#if defined(BENZIN_DEBUG_BUILD)
        void EnableD3D12DebugLayer()
        {
            ComPtr<ID3D12Debug5> d3d12Debug;
            BENZIN_HR_ASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));
            d3d12Debug->EnableDebugLayer();
            d3d12Debug->SetEnableGPUBasedValidation(true);
            d3d12Debug->SetEnableSynchronizedCommandQueueValidation(true);
            d3d12Debug->SetEnableAutoName(true);
        }
#endif

    } // anonymous namespace

    Backend::Backend()
    {
#if defined(BENZIN_DEBUG_BUILD)
        EnableD3D12DebugLayer();
#endif

        CreateDXGIFactory();
        CreateDXGIMainAdapter();
    }

    Backend::~Backend()
    {
        dx::SafeRelease(m_MainDXGIAdapter);
        dx::SafeRelease(m_DXGIFactory);
    }

    void Backend::CreateDXGIFactory()
    {
        UINT dxgiFactoryFlags = 0;
#if defined(BENZIN_DEBUG_BUILD)
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

        ComPtr<IDXGIFactory2> dxgiFactory2;
        BENZIN_HR_ASSERT(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory2)));
        BENZIN_HR_ASSERT(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&m_DXGIFactory)));
    }

    void Backend::CreateDXGIMainAdapter()
    {
        ComPtr<IDXGIAdapter> dxgiAdapter;
        BENZIN_HR_ASSERT(m_DXGIFactory->EnumAdapterByGpuPreference(
            config::GetMainAdapterIndex(),
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&dxgiAdapter)
        ));
        BENZIN_HR_ASSERT(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&m_MainDXGIAdapter)));

        DXGI_ADAPTER_DESC3 dxgiAdapterDesc;
        m_MainDXGIAdapter->GetDesc3(&dxgiAdapterDesc);

        BENZIN_TRACE(
            "\n"
            "==============================================\n"
            "Main Adapter\n"
            "{}\n"
            "VendorID: {}\n"
            "DeviceID: {}\n"
            "DedicatedVideoMemory: {}MB, {}GB\n"
            "DedicatedSystemMemory: {}MB, {}GB\n"
            "SharedSystemMemory: {}MB, {}GB\n"
            "==============================================",
            ToNarrowString(dxgiAdapterDesc.Description),
            dxgiAdapterDesc.VendorId,
            dxgiAdapterDesc.DeviceId,
            ConvertBytesToMB(dxgiAdapterDesc.DedicatedVideoMemory), ConvertBytesToGB(dxgiAdapterDesc.DedicatedVideoMemory),
            ConvertBytesToMB(dxgiAdapterDesc.DedicatedSystemMemory), ConvertBytesToGB(dxgiAdapterDesc.DedicatedSystemMemory),
            ConvertBytesToMB(dxgiAdapterDesc.SharedSystemMemory), ConvertBytesToGB(dxgiAdapterDesc.SharedSystemMemory)
        );
    }

} // namespace benzin
