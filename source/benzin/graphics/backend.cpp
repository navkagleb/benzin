#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/backend.hpp"

#include "benzin/graphics/common.hpp"

namespace benzin
{

    namespace
    {

#if BENZIN_IS_DEBUG_BUILD
        void EnableD3D12DebugLayer(const DebugLayerParams& params)
        {
            ComPtr<ID3D12Debug5> d3d12Debug;
            BenzinAssert(D3D12GetDebugInterface(IID_PPV_ARGS(&d3d12Debug)));

            d3d12Debug->EnableDebugLayer();
            d3d12Debug->SetEnableGPUBasedValidation(params.IsGPUBasedValidationEnabled);
            d3d12Debug->SetEnableSynchronizedCommandQueueValidation(params.IsSynchronizedCommandQueueValidationEnabled);
            d3d12Debug->SetEnableAutoName(true);

            BenzinTrace("------------------------------------------------------------------------");
            BenzinTrace("D3D12DebugLayer enabled");
            BenzinTrace("GPUBasedValidation enabled: {}", params.IsGPUBasedValidationEnabled);
            BenzinTrace("SynchronizedCommandQueueValidation enabled: {}", params.IsSynchronizedCommandQueueValidationEnabled);
            BenzinTrace("AutoName enabled: true");
            BenzinTrace("------------------------------------------------------------------------");
        }
#endif

    } // anonymous namespace

    Backend::Backend(const BackendCreation& creation)
    {
#if BENZIN_IS_DEBUG_BUILD
        EnableD3D12DebugLayer(creation.DebugLayerParams);
#endif

        CreateDXGIFactory();

        LogAdapters();
        CreateDXGIMainAdapter(creation.MainAdapterIndex);
    }

    Backend::~Backend()
    {
        SafeUnknownRelease(m_MainDXGIAdapter);
        SafeUnknownRelease(m_DXGIFactory);
    }

    void Backend::CreateDXGIFactory()
    {
        uint32_t dxgiFactoryFlags = 0;

#if BENZIN_IS_DEBUG_BUILD
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

        ComPtr<IDXGIFactory2> dxgiFactory2;
        BenzinAssert(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory2)));
        BenzinAssert(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&m_DXGIFactory)));
    }

    void Backend::LogAdapters()
    {
        BenzinTrace("Avaiable adapters:");

        for (uint32_t adapterIndex = 0; true; ++adapterIndex)
        {
            ComPtr<IDXGIAdapter> dxgiAdapter;
            if (FAILED(m_DXGIFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&dxgiAdapter))))
            {
                break;
            }

            DXGI_ADAPTER_DESC dxgiAdapterDesc;
            dxgiAdapter->GetDesc(&dxgiAdapterDesc);

            BenzinTrace("Adapter {}: {}", adapterIndex, ToNarrowString(dxgiAdapterDesc.Description));
        }
    }

    void Backend::CreateDXGIMainAdapter(uint32_t adapterIndex)
    {
        ComPtr<IDXGIAdapter> dxgiAdapter;
        BenzinAssert(m_DXGIFactory->EnumAdapterByGpuPreference(
            adapterIndex,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&dxgiAdapter)
        ));
        BenzinAssert(dxgiAdapter->QueryInterface(IID_PPV_ARGS(&m_MainDXGIAdapter)));

        DXGI_ADAPTER_DESC3 dxgiAdapterDesc;
        m_MainDXGIAdapter->GetDesc3(&dxgiAdapterDesc);

        m_MainAdapterName = ToNarrowString(dxgiAdapterDesc.Description);

        BenzinTrace("------------------------------------------------------------------------");
        BenzinTrace("Main Adapter:");
        BenzinTrace("{}", m_MainAdapterName);
        BenzinTrace("VendorID: {}", dxgiAdapterDesc.VendorId);
        BenzinTrace("DeviceID: {}", dxgiAdapterDesc.DeviceId);
        BenzinTrace("DedicatedVideoMemory: {}MB, {}GB", BytesToMB(dxgiAdapterDesc.DedicatedVideoMemory), BytesToGB(dxgiAdapterDesc.DedicatedVideoMemory));
        BenzinTrace("DedicatedSystemMemory: {}MB, {}GB", BytesToMB(dxgiAdapterDesc.DedicatedSystemMemory), BytesToGB(dxgiAdapterDesc.DedicatedSystemMemory));
        BenzinTrace("SharedSystemMemory: {}MB, {}GB", BytesToMB(dxgiAdapterDesc.SharedSystemMemory), BytesToGB(dxgiAdapterDesc.SharedSystemMemory));
        BenzinTrace("------------------------------------------------------------------------");
    }

} // namespace benzin
