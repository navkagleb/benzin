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

    struct MeshNode
    {
        float4x4 WorldTransform;
        float4x4 InverseWorldTransform;
    };

    struct MeshInstance
    {
        uint MeshIndex;
        uint MaterialIndex;
        uint NodeIndex;
    };

    struct Transform
    {
        float4x4 WorldMatrix;
        float4x4 InverseWorldMatrix;
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
    };

} // namespace joint
