#pragma once

#include "hlsl_to_cpp.hpp"
#include "light.hpp"

namespace joint
{

    static const uint g_SigmaTileSize = 16;

    struct SigmaConsts
    {
        float4 BlurRotator;
        float4 PostBlurRotator;
        uint2 TileCount;
        float StabilizationStrength;
        float PlaneDistanceSensitivity;
        float DisocclusionThreshold;
        uint IsTileSmoothingEnabled;
    };

    struct SigmaPerLightConsts
    {
        LightType LightType;
        float3 WorldLightPosition; // For sun - to sun direction
    };

    enum class SigmaClassifyTilesResources : uint
    {
        ViewDepth,
        Penumbra,

        OutTiles,
    };

    enum class SigmaSmoothTilesResources : uint
    {
        Tiles,

        OutSmoothTiles,
    };

    enum class SigmaBlurResources : uint
    {
        WorldNormal,
        ViewDepth,
        SmoothTiles,
        Penumbra,
        Shadow, // POST_BLUR_PASS

        OutPenumbra,
        OutShadow,
    };

    enum class SigmaTemporalStabilizationResources : uint
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

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::SigmaConsts
#endif

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType1)
    #define BenzinRenderPassConstsType1 joint::SigmaPerLightConsts
#endif
