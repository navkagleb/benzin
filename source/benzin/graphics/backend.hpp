#pragma once

namespace benzin
{

    enum class AdapterVendorId : uint32_t
    {
        Amd = 0x1002,
        Nvidia = 0x10DE,
        Other = g_Bad32,
    };

    struct AdapterInfo
    {
        std::string m_Name;

        uint32_t m_VendorId = g_Bad32;
        uint32_t m_DeviceId = g_Bad32;

        uint64_t m_TotalLocalVramInBytes = 0;
        uint64_t m_TotalHostVramInBytes = 0;

        uint32_t m_GpuCoreCount = g_Bad32;

        bool IsAmd() const { return m_VendorId == (uint32_t)AdapterVendorId::Amd ;}
        bool IsNvidia() const { return m_VendorId == (uint32_t)AdapterVendorId::Nvidia; }
        bool IsOther() const { return m_VendorId == (uint32_t)AdapterVendorId::Other; }
    };

    struct AdapterMemoryInfo
    {
        // Query from DXGI
        uint64_t m_LocalVramBudgetInBytes = 0;
        uint64_t m_UsedLocalVramInBytes = 0;

        uint64_t m_HostVramBudgetInBytes = 0;
        uint64_t m_UsedHostVramInBytes = 0;

        // Query from ADL or NvAPI
        uint64_t m_TotalUsedVramInBytes = 0;
        uint64_t m_AvailableVramInBytes = 0;
        uint64_t m_AvailableVramRelativeToOsBudgetInBytes = 0;
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
