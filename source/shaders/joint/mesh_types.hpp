#pragma once

#include "hlsl_to_cpp.hpp"

#if defined(__cplusplus)
    #include "benzin/core/common.hpp"
#endif

namespace joint
{

    // Note: StructuredBuffer alignment is different from ConstantBuffer alignment!

    struct MeshVertex
    {
        float3 Position;
        float3 Normal;
        float2 Uv;
    };

    struct MeshInstance
    {
        uint SubMeshIndex BenzinCppOnly( = 0);
        uint MaterialIndex BenzinCppOnly( = 0);
        uint2 _Padding; // This is mandatory! Despite the fact that there is already a gap there, there is none on the HLSL side. So force it
        float4x4 Transform BenzinCppOnly( = DirectX::XMMatrixIdentity());
    };

    struct MeshTransform
    {
        float4x4 LocalToWorld BenzinCppOnly( = DirectX::XMMatrixIdentity());
        float4x4 PrevLocalToWorld BenzinCppOnly( = DirectX::XMMatrixIdentity());
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

}
