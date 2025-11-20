#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class ComputeCullingResources : uint
    {
        MeshDrawCount,
        MeshDraws,
        Meshes,

        MeshDrawCmds,
        MeshDrawCmdCounter,
        VisibilityBuffer,
    };

    enum class GeometryResources : uint
    {
        MeshDrawIndex,

        MeshDraws,
        Materials,

        MeshDispatchCount, // Mesh pipeline
        MeshDispathes, // Mesh pipeline
        Vertices, // Mesh pipeline
        Meshlets, // Mesh pipeline
        MeshletCullVolumes, // Mesh pipeline
        MeshletVertexIndices, // Mesh pipeline
        MeshletIndices, // Mesh pipeline
    };

}
