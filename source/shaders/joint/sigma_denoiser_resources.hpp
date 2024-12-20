#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    static const uint g_SigmaTileSize = 16;

    struct SigmaConstants
    {
        float StabilizationStrength;
        float3 WorldSunDirection;
        float4 BlurRotator;
        float4 PostBlurRotator;
        uint2 TileCount;
        float PlaneDistanceSensitivity;
        float DisocclusionThreshold;
        uint IsTileSmoothingEnabled;
    };

    enum class Rc_SigmaClassifyTiles : uint32_t
    {
        ViewDepth,
        Penumbra,

        OutTiles,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaClassifyTiles);

    enum class Rc_SigmaSmoothTiles : uint32_t
    {
        Tiles,

        OutSmoothTiles,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaSmoothTiles);

    enum class Rc_SigmaCopyHistory : uint32_t
    {
        SmoothTiles,
        ShadowHistory,
        HistoryLength,

        OutShadowHistory,
        OutHistoryLength,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaCopyHistory);

    enum Rc_SigmaBlur : uint32_t
    {
        WorldNormal,
        ViewDepth,
        SmoothTiles,
        Penumbra,
        Shadow, // !FIRST_BLUR_PASS

        OutPenumbra,
        OutShadow,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaBlur);

    enum class Rc_SigmaTemporalStabilization : uint32_t
    {
        Mv,
        ViewDepth,
        SmoothTiles,
        Penumbra,
        Shadow,
        ShadowHistory,
        HistoryLength,

        OutShadow,
        OutHistoryLength,
    };
    BenzinEnableUnaryPlusForEnum(Rc_SigmaTemporalStabilization)

}
