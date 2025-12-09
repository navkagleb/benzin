#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct RayTracingShadowConsts
    {
        uint m_IsShadowsEnabled : 1;
        uint m_IsBlueNoiseUsed : 1;
        uint m_IsNoiseAnimated : 1;
    };

    struct RayTracingShadowPayload
    {
        float m_DistanceToOccluder;
    };

    enum class RayTracingShadowResources : uint
    {
        WorldNormal,
        Depth,
        BlueNoise,
        NoisyPenumbra,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType)
    #define BenzinRenderPassConstsType joint::RayTracingShadowConsts
#endif
