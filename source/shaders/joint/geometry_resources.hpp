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
        MeshletCullVolumes,
        MeshletIndirectVertices,
        MeshletIndices,

        MeshletCount,
    };

    struct GeometryPassConsts
    {
        uint IsMeshletColoringEnabled;
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::GeometryPassConsts
#endif
