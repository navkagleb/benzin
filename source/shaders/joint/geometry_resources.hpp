#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class GeometryResources : uint
    {
        EntityTransforms,
        UnifiedMaterials,

        EntityTransformIndex,

        Vertices,
        Meshlets,
        MeshletVertices,
        MeshletTriangles,

        InstanceTransforms,
        InstanceTransformIndex,
        InstanceMaterialIndex,
    };

}
