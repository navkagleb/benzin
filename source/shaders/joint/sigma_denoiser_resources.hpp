#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    static const uint g_SigmaTileSize = 16;

    enum class Rc_SigmaClassifyTiles : uint32_t
    {
        ViewDepthTex,
        PenumbraTex,

        OutTilesTex,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaClassifyTiles);

    enum class Rc_SigmaSmoothTiles : uint32_t
    {
        TilesTex,

        OutSmoothTilesTex,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaSmoothTiles);

    enum Rc_SigmaBlur : uint32_t
    {
        WorldNormalTex,
        ViewDepthTex,
        PenumbraTex,
        SmoothTilesTex,
        ShadowTex,
        HistoryTex, // FIRST_BLUR_PASS

        OutHistoryTex, // FIRST_BLUR_PASS
        OutPenumbraTex,
        OutShadowTex,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaBlur);

    enum class Rc_SigmaTemporalStabilization : uint32_t
    {
        ViewDepthTex,
        MvTex,
        PenumbraTex,
        ShadowTex,
        HistoryTex,
        SmoothTilesTex,

        OutShadowTex,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaTemporalStabilization)

    struct SigmaConstants
    {
        uint2 TileCount;
        float StabilizationStrength;
        float3 WorldSunDirection;
        float4 BlurRotator;
        float4 PostBlurRotator;
        bool IsBicubicSamplingUsedForHistory;
    };

}
