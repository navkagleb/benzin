#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    // NOTE: StructuredBuffer alignment is different from ConstBuffer alignment!

    struct MeshVertex
    {
        float3 Position;
        float3 Normal;
        float2 Uv;
    };

    struct Meshlet
    {
        uint VertexOffset;
        uint IndexOffset;

        uint VertexCount;
        uint TriangleCount;
    };

    enum class MeshletConsts
    {
        MaxVertexCount = 64,
        MaxTriangleCount = 124, // Must be multiple of 4 (for meshoptimizer library)

        AsGroupSize = 32,
        MsGroupSize = 128,
    };

    struct EntityTransform
    {
        float4x4 LocalToWorld;
        float4x4 PrevLocalToWorld;
    };

    struct Material
    {
        uint AlbedoTextureHeapIndex;
        uint NormalTextureHeapIndex;
        uint MetallicRoughnessTextureHeapIndex;
        uint EmissiveTextureHeapIndex;

        float4 AlbedoFactor;
        float AlphaCutoff;
        float NormalScale;
        float MetalnessFactor;
        float RoughnessFactor;
        float OcclusionStrenght;
        float3 EmissiveFactor;
    };

}
