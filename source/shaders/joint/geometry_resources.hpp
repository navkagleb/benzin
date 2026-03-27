#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class GeometryCullingRootParam : uint
    {
        MeshDraws,
        Meshes,
        Hzb,

        MeshCmdCounter,
        MeshDrawCmds,
        MeshDispatchCmds,
        VisibilityBuffer,
    };

    struct GeometryCullConsts
    {
        uint m_IsMeshPipelineEnabled : 1;
        uint m_IsFrustumCullingEnabled : 1;
        uint m_IsOcclusionCullingEnabled : 1;

        uint m_MeshDrawCount;
        float m_P00;
        float m_P11;
        float m_NearZ;
    };

    enum class GeometryRootParam : uint
    {
        MeshDrawIndex,
        MeshletOffset, // Mesh pipeline
        MeshletCount, // Mesh pipeline

        MeshDraws,
        Materials,

        Vertices, // Mesh pipeline
        Meshlets, // Mesh pipeline
        MeshletCullVolumes, // Mesh pipeline
        MeshletVertexIndices, // Mesh pipeline
        MeshletIndices, // Mesh pipeline
    };

    enum class GeometryHzbRootParam : uint
    {
        SourceMip,
        DestMip,
    };

    struct GeometryHzbConsts
    {
        float2 m_DestMipTexelSize;
        uint m_IsSourceWidthOdd : 1;
        uint m_IsSourceHeightOdd : 1;
    };

}
