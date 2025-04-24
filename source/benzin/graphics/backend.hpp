#pragma once

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
        uint32_t DeviceId = g_Bad32;

        uint64_t TotalVramInBytes = 0;
        uint64_t TotalRamInBytes = 0;
        uint64_t TotalSharedRamInBytes = 0;

        bool IsAmd() const { return VendorType == AdapterVendorType::Amd ;}
        bool IsNvidia() const { return VendorType == AdapterVendorType::Nvidia; }
        bool IsOther() const { return VendorType == AdapterVendorType::Other; }
    };

    struct AdapterMemoryInfo
    {
        // Query from DXGI
        uint64_t VramOsBudgetInBytes = 0;
        uint64_t ProcessUsedVramInBytes = 0;

        uint64_t SharedRamOsBudgetInBytes = 0;
        uint64_t ProcessUsedSharedRamInBytes = 0;

        // Query from ADL or NvAPI
        uint64_t TotalUsedVramInBytes = 0;
        uint64_t AvailableVramInBytes = 0;
        uint64_t AvailableVramRelativeToOsBudgetInBytes = 0;
    };

    class Backend
    {
    public:
        Backend();
        ~Backend();

    public:
        auto* GetDxgiFactory() const { return m_DxgiFactory; }
        auto* GetDxgiMainAdapter() const { return m_DxgiAdapters[m_MainAdapterIndex]; }

        auto GetMainAdapterIndex() const { return m_MainAdapterIndex; }
        auto GetAdapterCount() const { return (uint32_t)m_DxgiAdapters.size(); }

        const AdapterInfo& GetAdapterInfo(uint32_t adapterIndex) const;
        AdapterMemoryInfo GetAdapterMemoryInfo(uint32_t adapterIndex) const;

        const auto& GetMainAdapterInfo() const { return GetAdapterInfo(m_MainAdapterIndex); }
        auto GetMainAdapterMemoryInfo() const { return GetAdapterMemoryInfo(m_MainAdapterIndex); }

    private:
        void CreateDxgiFactory();
        void GatherDxgiAdapters();

    private:
        IDXGIFactory7* m_DxgiFactory = nullptr;

        std::vector<IDXGIAdapter3*> m_DxgiAdapters;
        std::vector<AdapterInfo> m_AdaptersInfo;

        uint32_t m_MainAdapterIndex = g_Bad32;
    };

}
