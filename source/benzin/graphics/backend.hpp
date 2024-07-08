#pragma once

#include "benzin/graphics/shader_manager.hpp"

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

        Bytes64 TotalDedicatedVram;
        Bytes64 TotalDedicatedRam;
        Bytes64 TotalSharedRam;

        bool IsAmd() const { return VendorType == AdapterVendorType::Amd ;}
        bool IsNvidia() const { return VendorType == AdapterVendorType::Nvidia; }
        bool IsOther() const { return VendorType == AdapterVendorType::Other; }
    };

    struct AdapterMemoryInfo
    {
        // Query from DXGI
        Bytes64 DedicatedVramOsBudget;
        Bytes64 ProcessUsedDedicatedVram;

        Bytes64 SharedRamOsBudget;
        Bytes64 ProcessUsedSharedRam;

        // Query from ADL or NvAPI
        Bytes64 TotalUsedDedicatedVram;
        Bytes64 AvailableDedicatedVram;
        Bytes64 AvailableDedicatedVramRelativeToOsBudget;
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
        auto GetAdapterCount() const { return m_DxgiAdapters.size(); }

        const AdapterInfo& GetAdaptersInfo(uint32_t adapterIndex) const;
        AdapterMemoryInfo GetAdapterMemoryInfo(uint32_t adapterIndex) const;

        const auto& GetMainAdapterInfo() const { return GetAdaptersInfo(m_MainAdapterIndex); }
        auto GetMainAdapterMemoryInfo() const { return GetAdapterMemoryInfo(m_MainAdapterIndex); }

        auto& GetShaderManager() { return m_ShaderManager; }
        const auto& GetShaderManager() const { return m_ShaderManager; }

    private:
        void CreateDxgiFactory();
        void GatherDxgiAdapters();

    private:
        IDXGIFactory7* m_DxgiFactory = nullptr;

        std::vector<IDXGIAdapter3*> m_DxgiAdapters;
        std::vector<AdapterInfo> m_AdaptersInfo;

        uint32_t m_MainAdapterIndex = g_InvalidUnsigned<uint32_t>;

        ShaderManager m_ShaderManager;
    };

} // namespace benzin
