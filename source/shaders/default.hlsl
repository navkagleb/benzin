// WRITE_ONLY_INTERPOLATED_NORMALS
// USE_ONLY_DIFFUSEMAP_SAMPLE

#include "common.hlsli"

uint GetPassBufferIndex() { return g_RootConstants.Constant0; }
uint GetPassBufferOffset() { return g_RootConstants.Constant1; }

uint GetEntityBufferIndex() { return g_RootConstants.Constant2; }
uint GetEntityBufferOffset() { return g_RootConstants.Constant3; }

uint GetMaterialBufferIndex() { return g_RootConstants.Constant4; }

uint GetEnvironmentMapIndex() { return g_RootConstants.Constant5; }
uint GetShadowMapIndex() { return g_RootConstants.Constant6; }

struct VS_INPUT
{
    float3 LocalPosition : Position;
    float3 LocalNormal : Normal;
    float3 LocalTangent : Tangent;
    float2 TexCoord : TexCoord;
};

struct VS_OUTPUT
{
    float4 HomogeneousPosition : SV_Position;
    float4 HomogeneousShadowPosition : ShadowPosition;
    float3 WorldPosition : Position;
    float3 WorldNormal : Normal;
    float3 WorldTangent : Tangent;
    float2 TexCoord : TexCoord;

    nointerpolation uint MaterialIndex : MaterialIndex;
};

VS_OUTPUT VS_Main(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    StructuredBuffer<PassData> passStructuredBuffer = ResourceDescriptorHeap[GetPassBufferIndex()];
    StructuredBuffer<EntityData> entityStructuredBuffer = ResourceDescriptorHeap[GetEntityBufferIndex()];
    StructuredBuffer<MaterialData> materialStructuredBuffer = ResourceDescriptorHeap[GetMaterialBufferIndex()];

    PassData passData = passStructuredBuffer[GetPassBufferOffset()];
    EntityData entityData = entityStructuredBuffer[GetEntityBufferOffset() + instanceID];
    MaterialData materialData = materialStructuredBuffer[entityData.MaterialIndex];

    const float4 worldPosition = mul(float4(input.LocalPosition, 1.0f), entityData.World);

    VS_OUTPUT output = (VS_OUTPUT)0;
    output.WorldPosition = worldPosition.xyz;
    output.HomogeneousPosition = mul(worldPosition, passData.ViewProjection);
    output.WorldNormal = mul(input.LocalNormal, (float3x3)entityData.World);
    output.WorldTangent = mul(input.LocalTangent, (float3x3)entityData.World);
    output.TexCoord = mul(float4(input.TexCoord, 0.0f, 1.0f), materialData.Transform).xy;
    output.MaterialIndex = entityData.MaterialIndex;

    output.HomogeneousShadowPosition = mul(worldPosition, passData.ShadowTransform);

    return output;
}

float4 PS_Main(VS_OUTPUT input) : SV_Target
{
    StructuredBuffer<PassData> passStructuredBuffer = ResourceDescriptorHeap[GetPassBufferIndex()];
    StructuredBuffer<MaterialData> materialStructuredBuffer = ResourceDescriptorHeap[GetMaterialBufferIndex()];

    const PassData passData = passStructuredBuffer[GetPassBufferOffset()];

#if defined(WRITE_ONLY_INTERPOLATED_NORMALS)

    input.WorldNormal = normalize(input.WorldNormal);

    const float3 viewNormal = mul(input.WorldNormal, (float3x3)passData.View);
    return float4(viewNormal, 1.0f);

#else

    const MaterialData materialData = materialStructuredBuffer[NonUniformResourceIndex(input.MaterialIndex)];
    const Texture2D diffuseMap = ResourceDescriptorHeap[materialData.DiffuseMapIndex];
    const Texture2D normalMap = ResourceDescriptorHeap[materialData.NormalMapIndex];

    float4 diffuseAlbedo = diffuseMap.Sample(g_LinearWrapSampler, input.TexCoord);

#if defined(USE_ONLY_DIFFUSEMAP_SAMPLE)
    return diffuseAlbedo;
#endif

    float4 litColor = 0.0f;

    const float3 viewVector = normalize(passData.WorldCameraPosition - input.WorldPosition);

    diffuseAlbedo *= materialData.DiffuseAlbedo;

    light::Material material;
    material.DiffuseAlbedo = diffuseAlbedo;
    material.FresnelR0 = materialData.FresnelR0;
    material.Shininess = 1.0f - materialData.Roughness;

    input.WorldNormal = normalize(input.WorldNormal);

    const float4 normalMapSample = normalMap.Sample(g_AnisotropicWrapSampler, input.TexCoord);
    const float3 worldBumpedNormal = NormalSampleToWorldSpace(normalMapSample.rgb, input.WorldNormal, input.WorldTangent);

    // Ligting
    {
        const float4 ambientLight = passData.AmbientLight * diffuseAlbedo;

        float3 shadowFactor = 1.0f;
        shadowFactor.x = CalculateShadowFactor(input.HomogeneousShadowPosition, GetShadowMapIndex());

        const float4 light = light::ComputeSunLight(
            passData.SunLight,
            material, 
            input.WorldPosition, 
            worldBumpedNormal, 
            viewVector, 
            shadowFactor
        );

        litColor = ambientLight + light;
    }

    // Specular reflections
    {
        const TextureCube cubeMap = ResourceDescriptorHeap[GetEnvironmentMapIndex()];

        const float3 reflection = reflect(-viewVector, worldBumpedNormal);
        const float4 reflectionColor = cubeMap.Sample(g_LinearWrapSampler, reflection);
        const float3 fresnelFactor = light::internal::SchlickFresnel(materialData.FresnelR0, worldBumpedNormal, reflection);

        const float3 specularReflections = material.Shininess * fresnelFactor * reflectionColor.rgb;

        litColor.rgb += specularReflections;
    }

    litColor.a = diffuseAlbedo.a;

    return litColor;

#endif // defined(WRITE_ONLY_INTERPOLATED_NORMALS)
}
