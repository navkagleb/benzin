#include "light.fx"

struct PerPass
{
    float4x4 View;
    float4x4 Projection;
    float3 W_CameraPosition;
    float4 AmbientLight;
    Light::LightContainer Lights;
    float4 FogColor;
    float FogStart;
    float FogRange;
};

struct PerObject
{
    float4x4 World;
    float4x4 TextureTransform;
};

struct PerMaterial
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 Transform;
};

ConstantBuffer<PerPass> g_Pass : register(b0);
ConstantBuffer<PerObject> g_Object : register(b1);
ConstantBuffer<PerMaterial> g_Material : register(b2);

struct VS_Input
{
    float3 L_Position : Position;
    float3 L_Normal : Normal;
    float2 TexCoord : TexCoord;
};

struct VS_Output
{
    float4 H_Position : SV_Position;
    float3 W_Position : Position;
    float3 W_Normal : Normal;
    float2 TexCoord : TexCoord;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output;
    output.W_Position = mul(float4(input.L_Position, 1.0f), g_Object.World).xyz;
    output.W_Normal = mul(input.L_Normal, (float3x3) g_Object.World);

    float4 position = float4(output.W_Position, 1.0f);
    position = mul(position, g_Pass.View);
    position = mul(position, g_Pass.Projection);

    output.H_Position = position;

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
    
    input.W_Normal = normalize(input.W_Normal);

    // Vector from point being lit to eye
    const float3 toCameraVector = g_Pass.W_CameraPosition - input.W_Position;
    const float distanceToCamera = length(toCameraVector);
    const float3 viewVector = toCameraVector / distanceToCamera;

    // Indirect lighting
    const float4 ambient = g_Pass.AmbientLight * diffuseAlbedo;

    // Direct lighting
    Light::Material material;
    material.DiffuseAlbedo = diffuseAlbedo;
    material.FresnelR0 = g_Material.FresnelR0;
    material.Shininess = 1.0f - g_Material.Roughness;

    const float3 shadowFactor = 1.0f;

    float4 litColor = Light::ComputeLighting(g_Pass.Lights, material, input.W_Position, input.W_Normal, viewVector, shadowFactor);
    litColor += ambient;

    float fogAmount = saturate((distanceToCamera - g_Pass.FogStart) / g_Pass.FogRange);
    litColor = lerp(litColor, g_Pass.FogColor, fogAmount);
    
    litColor.a = diffuseAlbedo.a;

    return litColor;
}