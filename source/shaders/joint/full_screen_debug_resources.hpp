#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class DebugOutputType : uint
    {
        None,

        GBuffer_Albedo,
        GBuffer_Roughness,
        GBuffer_Emissive,
        GBuffer_Metallic,
        GBuffer_WorldNormal,
        GBuffer_Mv,
        GBuffer_ViewDepth,

        NoisyPenumbra,

        SigmaTiles,
        SigmaSmoothTiles,
        SigmaPenumbra1,
        SigmaPenumbra2,
        SigmaShadowTemp1,
        SigmaShadowTemp2,
        SigmaShadow,
    };

    struct FullScreenDebugConsts
    {
        DebugOutputType OutputType;
        uint ViewDepthMipIndex;
        float MinViewDepth;
        float MaxViewDepth;
    };

    enum class Rc_FullScreenDebug : uint
    {
        AlbedoAndRoughness,
        EmissiveAndMetallic,
        WorldNormal,
        Mv,
        ViewDepth,
        Depth,
        NoisyPenumbra,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::FullScreenDebugConsts
#endif
