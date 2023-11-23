#pragma once

#include "benzin/graphics/device.hpp"

namespace benzin
{

    struct BackendCreation
    {
        uint32_t MainAdapterIndex = config::g_MainAdapterIndex;

#if BENZIN_IS_DEBUG_BUILD
        DebugLayerParams DebugLayerParams;
#endif
    };

    class Backend
    {
    public:
        explicit Backend(const BackendCreation& creation = {});
        ~Backend();

    public:
        IDXGIFactory7* GetDXGIFactory() const { return m_DXGIFactory; }
        IDXGIAdapter4* GetDXGIMainAdapter() const { return m_MainDXGIAdapter; }

        const std::string& GetMainAdapterName() const { return m_MainAdapterName; }

    private:
        void CreateDXGIFactory();

        void LogAdapters();
        void CreateDXGIMainAdapter(uint32_t adapterIndex);

    private:
        IDXGIFactory7* m_DXGIFactory = nullptr;
        IDXGIAdapter4* m_MainDXGIAdapter = nullptr;

        std::string m_MainAdapterName;
    };

} // namespace benzin
