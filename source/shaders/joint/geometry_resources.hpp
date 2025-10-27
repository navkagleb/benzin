#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class GeometryResources : uint
    {
        MeshDrawIndex,
        MeshDrawPartIndex,

        PartMeshletOffset, // Mesh pipeline
        PartMeshletCount, // Mesh pipeline

        MeshDraws,
        MeshDrawParts,
        Materials,

        Vertices, // Mesh pipeline
        Meshlets, // Mesh pipeline
        MeshletCullVolumes, // Mesh pipeline
        MeshletVertexIndices, // Mesh pipeline
        MeshletIndices, // Mesh pipeline
    };

}
