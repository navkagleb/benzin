#pragma once

#include "benzin/graphics/format.hpp"

namespace benzin::config
{

    constexpr uint32_t GetBackBufferCount() { return 3; }
    constexpr GraphicsFormat GetBackBufferFormat() { return GraphicsFormat::R8G8B8A8UnsignedNorm; }

    constexpr const char* GetVertexShaderTarget() { return "vs_5_1"; }
    constexpr const char* GetHullShaderTarget() { return "hs_5_1"; }
    constexpr const char* GetDomainShaderTarget() { return "ds_5_1"; }
    constexpr const char* GetGeometryShaderTarget() { return "gs_5_1"; }
    constexpr const char* GetPixelShaderTarget() { return "ps_5_1"; }
    constexpr const char* GetComputeShaderTarget() { return "cs_5_1"; }

} // namespace benzin::config
