#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class GeometryResources : uint
    {
        MeshTransforms,

        // Per mesh
        MeshTransformIndex,
        SubMeshInstances,
        Materials,

        // Per mesh instance
        SubMeshInstanceIndex,
    };

}
