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

    struct MeshDrawPart
    {
        float4x4 m_ObjectToLocal;
        uint32_t m_MaterialIndex;
        float3 m_Padding0;
    };

    struct MeshDraw
    {
        float4x4 m_LocalToWorld;
        float4x4 m_PrevLocalToWorld;
    };

    struct Meshlet
    {
        uint m_VertexOffset;
        uint m_VertexCount;
        uint m_IndexOffset;
        uint m_TriangleCount;
    };

    struct MeshletCullVolume
    {
        float3 m_Center;
        float m_Radius;

        float3 m_ConeApex;
        uint m_PackedAxisAndCutoff; // 3x int8_t axis + 1x int8_t cutoff
    };

    enum class MeshletConsts
    {
        MaxVertexCount = 64,
        MaxTriangleCount = 124, // Must be multiple of 4 (for meshoptimizer library)

        AsGroupSize = 32,
        MsGroupSize = 128,
    };

    struct Material
    {
        uint m_AlbedoTextureHeapIndex;
        uint m_NormalTextureHeapIndex;
        uint m_MetallicRoughnessTextureHeapIndex;
        uint m_EmissiveTextureHeapIndex;

        float4 m_AlbedoFactor;
        float m_AlphaCutoff;
        float m_NormalScale;
        float m_MetalnessFactor;
        float m_RoughnessFactor;
        float3 m_EmissiveFactor;
        float m_Padding0;
    };

}
