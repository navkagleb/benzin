#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    // NOTE: StructuredBuffer alignment is different from ConstBuffer alignment!

    struct MeshVertex
    {
        float3 m_Position;
        float3 m_Normal;
        float2 m_Uv;
    };

    struct MeshLod
    {
        uint m_IndexOffset;
        uint m_IndexCount;

        uint m_MeshletOffset;
        uint m_MeshletCount;
    };

    struct Mesh
    {
        float3 m_Center;
        float m_Radius;

        uint m_VertexOffset;

        uint m_LodCount;
        MeshLod m_Lods[8];
    };

    struct MeshDraw
    {
        float4x4 m_LocalToWorld;
        float4x4 m_PrevLocalToWorld;
        uint m_MeshIndex;
        uint m_MaterialIndex;
        float m_LocalToWorldScale;
        float m_Padding0;
    };

    struct MeshDrawCmd
    {
        uint m_DrawIndex;

        // D3D12_DRAW_INDEXED_ARGUMENTS
        uint m_IndexCountPerInstance;
        uint m_InstanceCount;
        uint m_StartIndexLocation;
        uint m_BaseVertexLocation;
        uint m_StartInstanceLocation;
    };

    struct MeshDispatchCmd
    {
        uint m_DrawIndex;
        uint m_MeshletOffset;
        uint m_MeshletCount;

        // D3D12_DISPATCH_MESH_ARGUMENTS
        uint m_ThreadGroupCountX;
        uint m_ThreadGroupCountY;
        uint m_ThreadGroupCountZ;
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

        AmplificationGroupSize = 32,
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
