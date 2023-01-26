#pragma once

namespace spieler::config
{

    inline constexpr const char* GetVertexShaderTarget() { return "vs_5_1"; }
    inline constexpr const char* GetHullShaderTarget() { return "hs_5_1"; }
    inline constexpr const char* GetDomainShaderTarget() { return "ds_5_1"; }
    inline constexpr const char* GetGeometryShaderTarget() { return "gs_5_1"; }
    inline constexpr const char* GetPixelShaderTarget() { return "ps_5_1"; }
    inline constexpr const char* GetComputeShaderTarget() { return "cs_5_1"; }

} // namespace spieler::config
