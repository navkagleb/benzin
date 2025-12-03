#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class GeometryCullingResources : uint
    {
        MeshDrawCount,
        MeshDraws,
        Meshes,

        MeshCmdCounter,
        MeshDrawCmds,
        MeshDispatchCmds,
        VisibilityBuffer,
    };

    enum class GeometryResources : uint
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

}
