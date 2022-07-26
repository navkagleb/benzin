#include "light.hlsl"

struct PassData
{
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;
    float3 CameraPosition;
};

struct ObjectData
{
    float4x4 World;
    uint MaterialIndex;
};

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 Transform;
    uint DiffuseMapIndex;
    uint3 Padding;
};

struct VS_INPUT
{
    float3 Position : Position;
    float3 Normal : Normal;
    float2 TexCoord : TexCoord;
};

struct VS_OUTPUT
{
    float4 PositionH : SV_Position;
    float3 PositionW : Position;
    float3 NormalW : Normal;
    float2 TexCoord : TexCoord;
};

ConstantBuffer<PassData> g_Pass : register(b0);
ConstantBuffer<ObjectData> g_Object : register(b1);

StructuredBuffer<MaterialData> g_Materials : register(t0, space1);

VS_OUTPUT VS_Main(VS_INPUT input)
{
    MaterialData materialData = g_Materials[g_Object.MaterialIndex];

    VS_OUTPUT output = (VS_OUTPUT)0;
    output.PositionW = mul(input.Position, (float3x3)g_Object.World);
    output.PositionH = mul(mul(float4(input.Position, 1.0f), g_Object.World), g_Pass.ViewProjection);
    output.NormalW = mul(input.Normal, (float3x3)g_Object.World);
    output.TexCoord = mul(float4(input.TexCoord, 0.0f, 1.0f), materialData.Transform).xy;

    return output;
}

Texture2D g_DiffuseMaps[4] : register(t0);

SamplerState g_SamplerLinearWrap : register(s0);
SamplerState g_SamplerAnisotropicWrap : register(s1);

float4 PS_Main(VS_OUTPUT input) : SV_Target
{
    MaterialData materialData = g_Materials[g_Object.MaterialIndex];
    Texture2D diffuseMap = g_DiffuseMaps[materialData.DiffuseMapIndex];

    return diffuseMap.Sample(g_SamplerAnisotropicWrap, input.TexCoord);
}
