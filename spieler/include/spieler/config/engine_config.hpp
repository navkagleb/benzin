#pragma once

namespace spieler::config
{

    inline constexpr const char* GetVertexShaderTarget() { return "vs_5_1"; }
    inline constexpr const char* GetPixelShaderTarget() { return "ps_5_1"; }
    inline constexpr const char* GetGeometryShaderTarget() { return "gs_5_1"; }

} // namespace spieler::config