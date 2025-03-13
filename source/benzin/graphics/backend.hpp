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
        uint32_t DeviceId = g_InvalidUnsigned<uint32_t>;

        Bytes64 TotalVram;
        Bytes64 TotalRam;
        Bytes64 TotalSharedRam;

        bool IsAmd() const { return VendorType == AdapterVendorType::Amd ;}
        bool IsNvidia() const { return VendorType == AdapterVendorType::Nvidia; }
        bool IsOther() const { return VendorType == AdapterVendorType::Other; }
    };

    struct AdapterMemoryInfo
    {
        // Query from DXGI
        Bytes64 VramOsBudget;
        Bytes64 ProcessUsedVram;

        Bytes64 SharedRamOsBudget;
        Bytes64 ProcessUsedSharedRam;

        // Query from ADL or NvAPI
        Bytes64 TotalUsedVram;
        Bytes64 AvailableVram;
        Bytes64 AvailableVramRelativeToOsBudget;
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

        uint32_t m_MainAdapterIndex = g_InvalidUnsigned<uint32_t>;
    };

}
