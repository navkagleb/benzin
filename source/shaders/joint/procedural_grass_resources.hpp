#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class ProceduralGrassConsts
    {
        VertexCountPerBladeEdge = 4,
        VertexCountPerBlade = VertexCountPerBladeEdge * 2,
        TriangleCountPerBlade = 6,

        MaxVertexCountPerThreadGroup = 256,
        MaxBladeCountPerPatch = MaxVertexCountPerThreadGroup / VertexCountPerBlade,

        AsGroupSize = 32, // There is max amplification shader group size due to wave size (max == 32)
    };

    struct GrassPatch
    {
        float3 Pos;
        float3 Normal;
        float Height;
    };

    struct ProceduralGrassPassConsts
    {
        uint GrassPatchCount;
        uint IsFrustumCullingEnabled;

        float GrassPatchCullRadius;
        float GrassEndDistance;
        float SpacingInGrassPatch;
        float WindDirection;
        float BladeWidth;
        float _Padding;
        float3 BaseColor;
    };

    enum class ProceduralGrassResources : uint
    {
        GrassPatches,
        PerlinNoise,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::ProceduralGrassPassConsts
#endif
