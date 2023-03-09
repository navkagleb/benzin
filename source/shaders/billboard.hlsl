#include "light.hlsl"

struct PassCB
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

struct MaterialCB
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 Transform;
};

ConstantBuffer<PassCB> g_Pass : register(b0);
ConstantBuffer<MaterialCB> g_Material : register(b2);

struct VS_Input
{
    float3 PositionW : Position;
    float2 SizeW : Size;
};

struct VS_Output
{
    float3 CenterW : Position;
    float2 SizeW : Size;
};

VS_Output VS_Main(VS_Input input)
{
    VS_Output output = (VS_Output) 0;
    output.CenterW = input.PositionW;
    output.SizeW = input.SizeW;
    
    return output;
}

struct GS_Output
{
    float4 PositionH : SV_Position;
    float3 PositionW : Position;
    float3 NormalW : Normal;
    float2 TexCoord : TexCoord;
    uint PrimitiveID : SV_PrimitiveID;
};

[maxvertexcount(4)]
void GS_Main(point VS_Output input[1], uint primitiveID : SV_PrimitiveID, inout TriangleStream<GS_Output> triangleStream)
{
    const uint VERTEX_COUNT = 4;
    
    const VS_Output inputPoint = input[0];
    
    const float3 up = float3(0.0f, 1.0f, 0.0f);
    const float3 look = normalize(float3(g_Pass.CameraPositionW.x - inputPoint.CenterW.x, 0.0f, g_Pass.CameraPositionW.z - inputPoint.CenterW.z));
    const float3 right = cross(up, look);
    
    const float halfWidth = 0.5f * inputPoint.SizeW.x;
    const float halfHeight = 0.5f * inputPoint.SizeW.y;
    
    float4 positions[VERTEX_COUNT] =
    {
        float4(inputPoint.CenterW + halfWidth * right - halfHeight * up, 1.0f),
        float4(inputPoint.CenterW + halfWidth * right + halfHeight * up, 1.0f),
        float4(inputPoint.CenterW - halfWidth * right - halfHeight * up, 1.0f),
        float4(inputPoint.CenterW - halfWidth * right + halfHeight * up, 1.0f)
    };
    
    float2 texCoords[VERTEX_COUNT] =
    {
        float2(0.0f, 1.0f),
        float2(0.0f, 0.0f),
        float2(1.0f, 1.0),
        float2(1.0f, 0.0f)
    };

    [unroll]
    for (uint i = 0; i < VERTEX_COUNT; ++i)
    {
        GS_Output output;
        output.PositionH = mul(positions[i], g_Pass.View);
        output.PositionH = mul(output.PositionH, g_Pass.Projection);
        output.PositionW = positions[i].xyz;
        output.NormalW = look;
        output.TexCoord = texCoords[i];
        output.PrimitiveID = primitiveID;
        
        triangleStream.Append(output);
    }
}

Texture2DArray g_BillboardMapArray : register(t0);

SamplerState g_SamplerPointWrap : register(s0);
SamplerState g_SamplerPointClamp : register(s1);
SamplerState g_SamplerLinearWrap : register(s2);
SamplerState g_SamplerLinearClamp : register(s3);
SamplerState g_SamplerAnisotropicWrap : register(s4);
SamplerState g_SamplerAnisotropicClamp : register(s5);

float4 PS_Main(GS_Output input) : SV_Target
{
    const float3 uvw = float3(input.TexCoord, input.PrimitiveID % 3);
    
    float4 diffuseAlbedo = g_BillboardMapArray.Sample(g_SamplerAnisotropicWrap, uvw) * g_Material.DiffuseAlbedo;
    
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