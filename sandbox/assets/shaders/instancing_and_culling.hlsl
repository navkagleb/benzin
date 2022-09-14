#include "common.hlsl"

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

float4 PS_Main(VS_OUTPUT input) : SV_Target
{
    const MaterialData materialData = g_Materials[input.MaterialIndex];
    const Texture2D diffuseMap = g_DiffuseMaps[NonUniformResourceIndex(materialData.DiffuseMapIndex)];

    float4 diffuseAlbedo = diffuseMap.Sample(g_LinearWrapSampler, input.TexCoord);

#if !defined(USE_LIGHTING)

    return diffuseAlbedo;

#else
    float4 litColor = 0.0f;

    const float3 viewVector = normalize(g_Pass.CameraPositionW - input.PositionW);

    diffuseAlbedo *= materialData.DiffuseAlbedo;

    light::Material material;
    material.DiffuseAlbedo = diffuseAlbedo;
    material.FresnelR0 = materialData.FresnelR0;
    material.Shininess = 1.0f - materialData.Roughness;

    // Ligting
    {
        input.NormalW = normalize(input.NormalW);

        const float4 ambient = g_Pass.AmbientLight * diffuseAlbedo;

        const float3 shadowFactor = 1.0f;
        const float4 light = light::ComputeLighting(g_Pass.Lights, material, input.PositionW, input.NormalW, viewVector, shadowFactor);

        litColor = ambient + light;
    }

    // Specular reflections
    {
        const float3 reflection = reflect(-viewVector, input.NormalW);
        const float4 reflectionColor = g_CubeMap.Sample(g_LinearWrapSampler, reflection);
        const float3 fresnelFactor = light::internal::SchlickFresnel(materialData.FresnelR0, input.NormalW, reflection);

        const float3 specularReflections = material.Shininess * fresnelFactor * reflectionColor.rgb;

        litColor.rgb += specularReflections;
    }

    litColor.a = diffuseAlbedo.a;

    return litColor;

#endif // !defined(USE_LIGHTING)
}
