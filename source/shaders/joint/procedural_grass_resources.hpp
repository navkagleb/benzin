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
        float3 m_Position;
        float3 m_Normal;
        float m_Height;
    };

    struct ProceduralGrassPassConsts
    {
        uint m_GrassPatchCount;
        uint m_IsFrustumCullingEnabled;

        float m_GrassPatchCullRadius;
        float m_GrassEndDistance;
        float m_SpacingInGrassPatch;
        float m_WindDirection;
        float m_BladeWidth;
        float m_Padding0;
        float3 m_BaseColor;
    };

    enum class ProceduralGrassResources : uint
    {
        GrassPatches,
        PerlinNoise,
    };

}
