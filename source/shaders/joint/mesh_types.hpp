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
        float3 Position BenzinCppOnly({});
        float3 Normal BenzinCppOnly({});
        float2 Uv BenzinCppOnly({});
    };

    struct MeshInfo
    {
        uint VertexOffset BenzinCppOnly( = 0);
        uint IndexOffset BenzinCppOnly( = 0);
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
        uint AlbedoTextureIndex BenzinCppOnly( = benzin::g_InvalidUnsigned<uint>);
        uint NormalTextureIndex BenzinCppOnly( = benzin::g_InvalidUnsigned<uint>);
        uint MetallicRoughnessTextureIndex BenzinCppOnly( = benzin::g_InvalidUnsigned<uint>);
        uint EmissiveTextureIndex BenzinCppOnly( = benzin::g_InvalidUnsigned<uint>);

        float4 AlbedoFactor BenzinCppOnly({ 1.0f, 1.0f, 1.0f, 1.0f });
        float AlphaCutoff BenzinCppOnly( = 0.0f);
        float NormalScale BenzinCppOnly( = 1.0f);
        float MetalnessFactor BenzinCppOnly( = 1.0f);
        float RoughnessFactor BenzinCppOnly( = 1.0f);
        float OcclusionStrenght BenzinCppOnly( = 1.0f);
        float3 EmissiveFactor BenzinCppOnly({ 0.0f, 0.0f, 0.0f });
    };

}
