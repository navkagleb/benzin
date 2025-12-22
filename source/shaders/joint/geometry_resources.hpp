#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class GeometryCullingRootParam : uint
    {
        MeshDrawCount,
        MeshDraws,
        Meshes,

        MeshCmdCounter,
        MeshDrawCmds,
        MeshDispatchCmds,
        VisibilityBuffer,
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
