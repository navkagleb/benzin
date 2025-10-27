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
    };

    enum class GeometryMeshResources : uint
    {
        PartMeshletCount = (uint)GeometryResources::Materials + 1,
        PartMeshletOffset,

        Vertices,
        Meshlets,
        MeshletCullVolumes,
        MeshletIndirectVertices,
        MeshletIndices,
    };

}
