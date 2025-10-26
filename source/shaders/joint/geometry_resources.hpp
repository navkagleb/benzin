#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class GeometryResources : uint
    {
        MeshDrawIndex,
        MeshDrawPartIndex,

        MeshDraws,
        MeshDrawParts,
        Materials,

        Vertices, // Mesh pipeline
        Meshlets, // Mesh pipeline
        MeshletCullVolumes, // Mesh pipeline
        MeshletIndirectVertices, // Mesh pipeline
        MeshletIndices, // Mesh pipeline
        MeshletCountPerInstance, // Mesh pipeline
        TotalMeshletCount, // Mesh pipeline

        InstanceIndex, // Debug
        MeshletIndex, // Debug
    };

}
