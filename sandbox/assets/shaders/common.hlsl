#include "light.hlsl"

struct PassData
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
    float4x4 ShadowTransform;
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
Texture2D g_ShadowMap : register(t1);

Texture2D g_Textures[10] : register(t2);

SamplerState g_PointWrapSampler : register(s0);
SamplerState g_PointClampSampler : register(s1);
SamplerState g_LinearWrapSampler : register(s2);
SamplerState g_LinearClampSampler : register(s3);
SamplerState g_AnisotropicWrapSampler : register(s4);
SamplerState g_AnisotropicClampSampler : register(s5);
SamplerComparisonState g_ShadowSampler : register(s6);

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

// Return value from [0, 1]
float CalculateShadowFactor(float4 shadowPositionH)
{
    // Useless for orthographic projection
    shadowPositionH.xyz /= shadowPositionH.w;

    const float2 shadowTexCoord = shadowPositionH.xy;
    const float depth = shadowPositionH.z;

    uint width;
    uint height;
    uint mipCount;
    g_ShadowMap.GetDimensions(0, width, height, mipCount);

    const float texelSize = 1.0f / (float)width;
    const float2 offsets[9] =
    {
        float2(-texelSize, -texelSize), float2(0.0f,  -texelSize),  float2(texelSize, -texelSize),
        float2(-texelSize, 0.0f),       float2(0.0f, 0.0f),         float2(texelSize, 0.0f),
        float2(-texelSize, texelSize),  float2(0.0f, texelSize),    float2(texelSize, texelSize)
    };

    float percentLit = 0.0f;

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler, shadowTexCoord + offsets[i], depth).r;
    }

    return percentLit / 9.0f;
}
