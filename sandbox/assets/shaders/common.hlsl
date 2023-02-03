#include "light.hlsl"

struct PassData
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
    float3 CameraPositionW;

    float4 AmbientLight;
    light::LightContainer Lights;
};

struct EntityData
{
    float4x4 World;
    uint MaterialIndex;
    uint3 Padding;
};

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 Transform;
    uint DiffuseMapIndex;
    uint NormalMapIndex;
    uint2 Padding;
};

ConstantBuffer<PassData> g_Pass : register(b0);

StructuredBuffer<EntityData> g_Entities : register(t1, space1);
StructuredBuffer<MaterialData> g_Materials : register(t1, space2);

TextureCube g_CubeMap : register(t0);

Texture2D g_Textures[10] : register(t1);

SamplerState g_PointWrapSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);
SamplerState g_LinearWrapSampler : register(s2);
SamplerState g_LinearClampSampler : register(s3);
SamplerState g_AnisotropicWrapSampler : register(s4);
SamplerState g_AnisotropicClampSampler : register(s5);

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    // [0, 1] -> [-1, 1]
    const float3 normalT = 2.0f * normalMapSample - 1.0f;

    // Orthonormal basis
    const float3 n = unitNormalW;
    const float3 t = normalize(tangentW - dot(tangentW, n) * n);
    const float3 b = -cross(n, t);

    const float3x3 tbn = float3x3(t, b, n);

    // Transform from tangent space to world space
    const float3 bumpedNormalW = mul(normalT, tbn);
    return bumpedNormalW;
}
