#include "light.hlsl"

// To prevent matrix transposition in CPU side
#pragma pack_matrix(row_major)

struct RootConstants
{
    uint Constant0;
    uint Constant1;
    uint Constant2;
    uint Constant3;
    uint Constant4;
    uint Constant5;
    uint Constant6;
    uint Constant7;
    uint Constant8;
    uint Constant9;

    uint Constant10;
    uint Constant11;
    uint Constant12;
    uint Constant13;
    uint Constant14;
    uint Constant15;
    uint Constant16;
    uint Constant17;
    uint Constant18;
    uint Constant19;

    uint Constant20;
    uint Constant21;
    uint Constant22;
    uint Constant23;
    uint Constant24;
    uint Constant25;
    uint Constant26;
    uint Constant27;
    uint Constant28;
    uint Constant29;

    uint Constant30;
    uint Constant31;
};

ConstantBuffer<RootConstants> g_RootConstants : register(b0, space0);

struct PassData
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
    float4x4 ShadowTransform;
    float3 WorldCameraPosition;
    uint _Padding;

    float4 AmbientLight;
    light::Light SunLight;
};

struct EntityData
{
    float4x4 World;
    uint MaterialIndex;
    uint3 _Padding;
};

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 Transform;
    uint DiffuseMapIndex;
    uint NormalMapIndex;
    uint2 _Padding;
};

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
float CalculateShadowFactor(float4 shadowPositionH, uint shadowMapIndex)
{
    const Texture2D shadowMap = ResourceDescriptorHeap[shadowMapIndex];

    // Useless for orthographic projection
    shadowPositionH.xyz /= shadowPositionH.w;

    const float2 shadowTexCoord = shadowPositionH.xy;
    const float depth = shadowPositionH.z;

    uint width;
    uint height;
    uint mipCount;
    shadowMap.GetDimensions(0, width, height, mipCount);

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
        percentLit += shadowMap.SampleCmpLevelZero(g_ShadowSampler, shadowTexCoord + offsets[i], depth).r;
    }

    return percentLit / 9.0f;
}
