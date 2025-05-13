#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class GeometryResources : uint
    {
        EntityTransforms,
        UnifiedMaterials,

        EntityTransformIndex,

        ObjectToLocalMatrices,
        ObjectToLocalMatrixIndex,
        MaterialIndex,

        Vertices,
        Meshlets,
        MeshletIndirectVertices,
        MeshletIndices,
    };

}
