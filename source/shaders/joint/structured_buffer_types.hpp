#pragma once

// #TODO: Include directory must be started from 'shaders'
// Undef hlsl defines in the end of the file
#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct MeshVertex
    {
        float3 Position;
        float3 Normal;
        float2 UV;
    };

    struct MeshInfo
    {
        uint VertexOffset;
        uint IndexOffset;
    };

    struct MeshInstance
    {
        uint MeshIndex;
        uint MaterialIndex;
        uint NodeIndex;
    };

    struct MeshTransform
    {
        float4x4 Matrix;
        float4x4 InverseMatrix;
    };

    struct Material
    {
        uint AlbedoTextureIndex;
        uint NormalTextureIndex;
        uint MetalRoughnessTextureIndex;
        uint EmissiveTextureIndex;

        float4 AlbedoFactor;
        float AlphaCutoff;
        float NormalScale;
        float MetalnessFactor;
        float RoughnessFactor;

        float OcclusionStrenght;
        float3 EmissiveFactor;
    };

    struct PointLight
    {
        float3 Color;
        float Intensity;
        float3 WorldPosition;
        float ConstantAttenuation;
        float LinearAttenuation;
        float ExponentialAttenuation;

        float GeometryRadius;
    };

    struct ShadowRayPayload
    {
        bool IsHitted;
    };

} // namespace joint
