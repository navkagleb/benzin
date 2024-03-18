#pragma once

#include "benzin/graphics/device.hpp"

namespace benzin
{

    enum class AdapterVendorType
    {
        Amd,
        Nvidia,
        Other,
    };

    struct AdapterInfo
    {
        std::string Name;

        AdapterVendorType VendorType = AdapterVendorType::Other;
        uint32_t DeviceId = g_InvalidIndex<uint32_t>;

        uint64_t TotalDedicatedVramInBytes = 0;
        uint64_t TotalDedicatedRamInBytes = 0;
        uint64_t TotalSharedRamInBytes = 0;

        bool IsAmd() const { return VendorType == AdapterVendorType::Amd ;}
        bool IsNvidia() const { return VendorType == AdapterVendorType::Nvidia; }
        bool IsOther() const { return VendorType == AdapterVendorType::Other; }
    };

    struct AdapterMemoryInfo
    {
        // Query from DXGI
        uint64_t DedicatedVramOsBudgetInBytes = 0;
        uint64_t ProcessUsedDedicatedVramInBytes = 0;

        uint64_t SharedRamOsBudgetInBytes = 0;
        uint64_t ProcessUsedSharedRamInBytes = 0;

        // Query from ADL or NvAPI
        uint64_t TotalUsedDedicatedVramInBytes = 0;
        uint64_t AvailableDedicatedVramInBytes = 0;
        uint64_t AvailableDedicatedVramRelativeToOsBudgetInBytes = 0;
    };

    class Backend
    {
    public:
        Backend();
        ~Backend();

    public:
        auto* GetDxgiFactory() const { return m_DxgiFactory; }
        auto* GetDxgiMainAdapter() const { return m_MainDxgiAdapter; }

        auto GetMainAdapterIndex() const { return m_MainAdapterIndex; }
        auto GetAdapterCount() const { return m_DxgiAdapters.size(); }

        const AdapterInfo& GetAdaptersInfo(uint32_t adapterIndex) const;
        const AdapterInfo& GetMainAdapterInfo() const;

        AdapterMemoryInfo GetAdapterMemoryInfo(uint32_t adapterIndex) const;
        AdapterMemoryInfo GetMainAdapterMemoryInfo() const;

    private:
        void CreateDxgiFactory();

        void GatherDxgiAdapters();
        void QueryDxgiMainAdapter();

    private:
        IDXGIFactory7* m_DxgiFactory = nullptr;
        IDXGIAdapter4* m_MainDxgiAdapter = nullptr;

        std::vector<IDXGIAdapter3*> m_DxgiAdapters;
        std::vector<AdapterInfo> m_AdaptersInfo;

        uint32_t m_MainAdapterIndex = g_InvalidIndex<uint32_t>;
    };

} // namespace benzin
