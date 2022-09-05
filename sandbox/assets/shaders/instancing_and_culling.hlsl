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

struct RenderItemData
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

    nointerpolation uint MaterialIndex : MaterialIndex;
};

ConstantBuffer<PassData> g_Pass : register(b0);

StructuredBuffer<RenderItemData> g_RenderItems : register(t0, space1);
StructuredBuffer<MaterialData> g_Materials : register(t0, space2);

VS_OUTPUT VS_Main(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    const RenderItemData renderItemData = g_RenderItems[instanceID];
    const MaterialData materialData = g_Materials[renderItemData.MaterialIndex];

    VS_OUTPUT output = (VS_OUTPUT)0;
    output.PositionW = mul(float4(input.Position, 1.0f), renderItemData.World).xyz;
    output.PositionH = mul(mul(float4(input.Position, 1.0f), renderItemData.World), g_Pass.ViewProjection);
    output.NormalW = mul(input.Normal, (float3x3)renderItemData.World);
    output.TexCoord = mul(float4(input.TexCoord, 0.0f, 1.0f), materialData.Transform).xy;
    output.MaterialIndex = renderItemData.MaterialIndex;

    return output;
}

Texture2D g_DiffuseMaps[4] : register(t0);

SamplerState g_SamplerLinearWrap : register(s0);
SamplerState g_SamplerAnisotropicWrap : register(s1);

float4 PS_Main(VS_OUTPUT input) : SV_Target
{
    const MaterialData materialData = g_Materials[input.MaterialIndex];
    const Texture2D diffuseMap = g_DiffuseMaps[NonUniformResourceIndex(materialData.DiffuseMapIndex)];

    float4 diffuseAlbedo = diffuseMap.Sample(g_SamplerLinearWrap, input.TexCoord);

#if !defined(USE_LIGHTING)

    return diffuseAlbedo;

#else

    diffuseAlbedo *= materialData.DiffuseAlbedo;

    input.NormalW = normalize(input.NormalW);

    const float3 viewVector = normalize(g_Pass.CameraPositionW - input.PositionW);

    const float4 ambient = g_Pass.AmbientLight * diffuseAlbedo;

    light::Material material;
    material.DiffuseAlbedo = diffuseAlbedo;
    material.FresnelR0 = materialData.FresnelR0;
    material.Shininess = 1.0f - materialData.Roughness;

    const float3 shadowFactor = 1.0f;

    float4 litColor = light::ComputeLighting(g_Pass.Lights, material, input.PositionW, input.NormalW, viewVector, shadowFactor);
    litColor += ambient;

    litColor.a = diffuseAlbedo.a;

    return litColor;

#endif // !defined(USE_LIGHTING)
}
