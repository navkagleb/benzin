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

ConstantBuffer<PassData> g_Pass : register(b0);

StructuredBuffer<RenderItemData> g_RenderItems : register(t1, space1);
StructuredBuffer<MaterialData> g_Materials : register(t1, space2);

TextureCube g_CubeMap : register(t0);

Texture2D g_DiffuseMaps[4] : register(t1);

//SamplerState g_SamplerPointWrap : register(s0);
//SamplerState g_SamplerPointClamp : register(s1);
SamplerState g_LinearWrapSampler : register(s0);
//SamplerState g_SamplerLinearClamp : register(s3);
SamplerState g_AnisotropicWrapSampler : register(s1);
//SamplerState g_SamplerAnisotropicClamp : register(s5);
