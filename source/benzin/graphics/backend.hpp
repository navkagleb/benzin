#pragma once

#include "benzin/graphics/device.hpp"

namespace benzin
{

    class Backend
    {
    public:
        Backend();
        ~Backend();

    public:
        IDXGIFactory7* GetDXGIFactory() const { return m_DXGIFactory; }
        IDXGIAdapter4* GetDXGIMainAdapter() const { return m_MainDXGIAdapter; }

        std::string_view GetMainAdapterName() const { return m_MainAdapterName; }

    private:
        void CreateDXGIFactory();

        void LogAdapters();
        void CreateDXGIMainAdapter();

    private:
        IDXGIFactory7* m_DXGIFactory = nullptr;
        IDXGIAdapter4* m_MainDXGIAdapter = nullptr;

        std::string m_MainAdapterName;
    };

} // namespace benzin
