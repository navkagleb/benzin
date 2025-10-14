#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class DebugColoringType
    {
        None,
        Primitive,
        Instance,
    };

    enum class GeometryResources : uint
    {
        UnifiedMaterials,
        ReprojectedHzb,

        Batch_LocalToWorldMatrices,
        Batch_PrevLocalToWorldMatrices,
        Batch_MaterialIndices,

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

    struct GeometryPassConsts
    {
        uint IsFrustumCullingEnabled : 1;
        uint IsBackfaceCullingEnabled : 1;
        uint IsOcclusionCullingEnabled : 1;

        DebugColoringType ColoringType;
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::GeometryPassConsts
#endif
