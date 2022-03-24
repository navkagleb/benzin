#include "light.fx"

struct PerPass
{
    float4x4 View;
    float4x4 Projection;
    float3 W_CameraPosition;
    float4 AmbientLight;
    Light::LightContainer Lights;
};

struct PerObject
{
    float4x4 World;
};

struct PerMaterial
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 Transform;
};

ConstantBuffer<PerPass> g_PerPass : register(b0);
ConstantBuffer<PerObject> g_PerObject : register(b1);
ConstantBuffer<PerMaterial> g_PerMaterial : register(b2);

struct VS_Input
{
    float3 L_Position : Position;
    float3 L_Normal : Normal;
};

struct VS_Output
{
    float4 H_Position : SV_Position;
    float3 W_Position : Position;
    float3 W_Normal : Normal;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output;
    output.W_Position = mul(float4(input.L_Position, 1.0f), g_PerObject.World).xyz;
    output.W_Normal = mul(input.L_Normal, (float3x3) g_PerObject.World);

    float4 position = float4(output.W_Position, 1.0f);
    position = mul(position, g_PerPass.View);
    position = mul(position, g_PerPass.Projection);

    output.H_Position = position;

    return output;
}

float4 PS_Main(VS_Output input) : SV_Target
{
    input.W_Normal = normalize(input.W_Normal);

    // Vector from point being lit to eye
    const float3 viewVector = normalize(g_PerPass.W_CameraPosition - input.W_Position);

    // Indirect lighting
    const float4 ambient = g_PerPass.AmbientLight * g_PerMaterial.DiffuseAlbedo;

    // Direct lighting
    Light::Material material;
    material.DiffuseAlbedo = g_PerMaterial.DiffuseAlbedo;
    material.FresnelR0 = g_PerMaterial.FresnelR0;
    material.Shininess = 1.0f - g_PerMaterial.Roughness;

    const float3 shadowFactor = 1.0f;

    float4 litColor = Light::ComputeLighting(g_PerPass.Lights, material, input.W_Position, input.W_Normal, viewVector, shadowFactor);
    litColor += ambient;

    litColor.a = g_PerMaterial.DiffuseAlbedo.a;

    return litColor;
}