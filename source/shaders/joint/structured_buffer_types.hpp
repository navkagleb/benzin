#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct MeshVertex
    {
        float3 Position;
        float3 Normal;
        float2 Uv;
    };

    struct MeshInfo
    {
        uint VertexOffset;
        uint IndexOffset;
    };

    struct MeshInstance
    {
        float4x4 Transform;
        uint MeshIndex;
        uint MaterialIndex;
        uint2 Padding_0;
    };

    struct MeshTransform
    {
        float4x4 WorldMatrix;
        float4x4 PreviousWorldMatrix;
    };

    struct Material
    {
        uint AlbedoTextureIndex;
        uint NormalTextureIndex;
        uint MetallicRoughnessTextureIndex;
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

}
