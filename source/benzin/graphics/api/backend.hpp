#pragma once

#include "benzin/graphics/api/device.hpp"

namespace benzin
{

    class Backend
    {
    public:
        Backend();
        ~Backend();

    public:
        IDXGIFactory7* GetDXGIFactory() const { BENZIN_ASSERT(m_DXGIFactory); return m_DXGIFactory; }
        IDXGIAdapter4* GetDXGIMainAdapter() const { BENZIN_ASSERT(m_MainDXGIAdapter); return m_MainDXGIAdapter; }

    private:
        void CreateDXGIFactory();
        void CreateDXGIMainAdapter();

    private:
        IDXGIFactory7* m_DXGIFactory{ nullptr };
        IDXGIAdapter4* m_MainDXGIAdapter{ nullptr };
    };

} // namespace benzin
