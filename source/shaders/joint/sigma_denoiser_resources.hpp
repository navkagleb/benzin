#pragma once

#include "hlsl_to_cpp.hpp"

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
