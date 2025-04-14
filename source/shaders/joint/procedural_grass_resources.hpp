#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class ProceduralGrassMsConsts
    {
        VertexCountPerBladeEdge = 4,
        VertexCountPerBlade = VertexCountPerBladeEdge * 2,
        TriangleCountPerBlade = 6,

        MaxVertexCountPerThreadGroup = 256,
        MaxBladeCountPerPatch = MaxVertexCountPerThreadGroup / VertexCountPerBlade,
    };

    struct ProceduralGrassConsts
    {
        float3 BaseColor;
        float GrassEndDistance;
        float WindDirection;
        float SpacingInPatch;
        float BladeWidth;
    };

    struct GrassPatch
    {
        float3 Pos;
        float3 Normal;
        float Height;
    };

    enum class ProceduralGrassResources : uint
    {
        GrassPatches,
        PerlinNoise,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::ProceduralGrassConsts
#endif
