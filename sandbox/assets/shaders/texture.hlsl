#include "light.hlsl"

struct PerPass
{
    float4x4 View;
    float4x4 Projection;
    float3 CameraPositionW;
    float4 AmbientLight;
    light::LightContainer Lights;
    
    float4 FogColor;
    float FogStart;
    float FogRange;
};

struct PerObject
{
    float4x4 World;
    float4x4 TextureTransform;
};

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 Transform;
};

ConstantBuffer<PerPass> g_Pass : register(b0);
ConstantBuffer<PerObject> g_Object : register(b1);
ConstantBuffer<Material> g_Material : register(b2);

struct VS_Input
{
    float3 PositionL : Position;
    float3 NormalL : Normal;
    float3 Tangent : Tangent;
    float2 TexCoord : TexCoord;
};

struct VS_Output
{
    float4 PositionH : SV_Position;
    float3 PositionW : Position;
    float3 NormalW : Normal;
    float2 TexCoord : TexCoord;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output = (VS_Output) 0;
    output.PositionW = mul(float4(input.PositionL, 1.0f), g_Object.World).xyz;
    output.NormalW = mul(input.NormalL, (float3x3) g_Object.World);

    float4 position = float4(output.PositionW, 1.0f);
    position = mul(position, g_Pass.View);
    position = mul(position, g_Pass.Projection);

    output.PositionH = position;
    
    float4 texCoord = mul(float4(input.TexCoord, 0.0f, 1.0f), g_Object.TextureTransform);
    output.TexCoord = mul(texCoord, g_Material.Transform).xy;
    
    return output;
}

Texture2D g_DiffuseMap : register(t0);

SamplerState g_SamplerPointWrap : register(s0);
SamplerState g_SamplerPointClamp : register(s1);
SamplerState g_SamplerLinearWrap : register(s2);
SamplerState g_SamplerLinearClamp : register(s3);
SamplerState g_SamplerAnisotropicWrap : register(s4);
SamplerState g_SamplerAnisotropicClamp : register(s5);

float4 PS_Main(VS_Output input) : SV_Target
{
    float4 diffuseAlbedo = g_DiffuseMap.Sample(g_SamplerAnisotropicWrap, input.TexCoord) * g_Material.DiffuseAlbedo;
    
#if defined(USE_ALPHA_TEST)
    clip(diffuseAlbedo.a - 0.1f);
#endif
    
    input.NormalW = normalize(input.NormalW);

    // Vector from point being lit to eye
    // Vector from point being lit to eye
    const float3 toCameraVector = g_Pass.CameraPositionW - input.PositionW;
    const float distanceToCamera = length(toCameraVector);
    const float3 viewVector = toCameraVector / distanceToCamera;

    // Indirect lighting
    const float4 ambient = g_Pass.AmbientLight * diffuseAlbedo;

    // Direct lighting
    light::Material material;
    material.DiffuseAlbedo = diffuseAlbedo;
    material.FresnelR0 = g_Material.FresnelR0;
    material.Shininess = 1.0f - g_Material.Roughness;

    const float3 shadowFactor = 1.0f;

    float4 litColor = light::ComputeLighting(g_Pass.Lights, material, input.PositionW, input.NormalW, viewVector, shadowFactor);
    litColor += ambient;
    
#if defined(ENABLE_FOG)
    float fogAmount = saturate((distanceToCamera - g_Pass.FogStart) / g_Pass.FogRange);
    litColor = lerp(litColor, g_Pass.FogColor, fogAmount);
#endif

    litColor.a = diffuseAlbedo.a;
    
    return litColor;
}