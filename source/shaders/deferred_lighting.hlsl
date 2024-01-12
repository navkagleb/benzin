#include "common.hlsli"
#include "fullscreen_helper.hlsli"
#include "gbuffer_utils.hlsli"
#include "pbr.hlsli"

struct DirectionalLight
{
    float3 Color;
    float Intensity;
    float3 WorldDirection;
};

struct Attenuation
{
    float Constant;
    float Linear;
    float Exponential;
};

struct PointLight
{
    float3 Color;
    float Intensity;
    float3 WorldPosition;
    Attenuation Attenuation;
};

float CalculateAttenuation(float distance, Attenuation attenuation)
{
    return 1.0f / (attenuation.Constant + attenuation.Linear * distance + attenuation.Exponential * distance * distance);
}

float3 GetLitColorForDirectionalLight(DirectionalLight directionalLight, pbr::Material material, float3 worldViewDirection, float3 worldNormal)
{
    pbr::Light light;
    light.Color = directionalLight.Color;
    light.Intensity = directionalLight.Intensity;
    light.Direction = normalize(-directionalLight.WorldDirection);

    return pbr::GetLitColor(light, material, worldViewDirection, worldNormal);
}

float3 GetLitColorForPointLight(PointLight pointLight, pbr::Material material, float3 worldPosition, float3 worldViewDirection, float3 worldNormal)
{
    float3 lightDirection = pointLight.WorldPosition - worldPosition;
    const float distance = length(lightDirection);

    lightDirection /= distance;

    pbr::Light light;
    light.Color = pointLight.Color;
    light.Intensity = pointLight.Intensity;
    light.Direction = lightDirection;

    const float attenuation = CalculateAttenuation(distance, pointLight.Attenuation);
    const float3 pbr = pbr::GetLitColor(light, material, worldViewDirection, worldNormal);

    return attenuation * pbr;
}

enum : uint32_t
{
    g_PassBufferIndex,

    g_AlbedoTextureIndex,
    g_WorldNormalTextureIndex,
    g_EmissiveTextureIndex,
    g_RoughnessMetalnessTextureIndex,
    g_DepthTextureIndex,

    g_PointLightBufferIndex,

    g_OutputType,
};

enum OutputType : uint32_t
{
    Final,
    ReconsructedWorldPosition,
    GBuffer_Depth,
    GBuffer_Albedo,
    GBuffer_WorldNormal,
    GBuffer_Emissive,
    GBuffer_Roughness,
    GBuffer_Metalness,
};

struct PassData
{
    float4x4 InverseViewMatrix;
    float4x4 InverseProjectionMatrix;
    float3 WorldCameraPosition;
    uint32_t PointLightCount;
    float3 SunColor;
    float SunIntensity;
    float3 SunDirection;
};

gbuffer::Unpacked ReadGBuffer(float2 uv)
{
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[BenzinGetRootConstant(g_AlbedoTextureIndex)];
    Texture2D<float4> worldNormal = ResourceDescriptorHeap[BenzinGetRootConstant(g_WorldNormalTextureIndex)];
    Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[BenzinGetRootConstant(g_EmissiveTextureIndex)];
    Texture2D<float4> roughnessMetalnessTexture = ResourceDescriptorHeap[BenzinGetRootConstant(g_RoughnessMetalnessTextureIndex)];

    gbuffer::Packed packed;
    packed.Color0 = albedoTexture.Sample(common::g_LinearWrapSampler, uv);
    packed.Color1 = worldNormal.Sample(common::g_LinearWrapSampler, uv);
    packed.Color2 = emissiveTexture.Sample(common::g_LinearWrapSampler, uv);
    packed.Color3 = roughnessMetalnessTexture.Sample(common::g_LinearWrapSampler, uv);

    return gbuffer::Unpack(packed);
}

float ReadDepth(float2 uv)
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[BenzinGetRootConstant(g_DepthTextureIndex)];
    return depthTexture.Sample(common::g_LinearWrapSampler, uv).r;
}

float4 PS_Main(fullscreen_helper::VS_Output input) : SV_Target
{
    const float depth = ReadDepth(input.UV);
    if (depth == 1.0f)
    {
        discard;
    }

    ConstantBuffer<PassData> passData = ResourceDescriptorHeap[BenzinGetRootConstant(g_PassBufferIndex)];
    StructuredBuffer<PointLight> pointLightBuffer = ResourceDescriptorHeap[BenzinGetRootConstant(g_PointLightBufferIndex)];

    const gbuffer::Unpacked gbuffer = ReadGBuffer(input.UV);

    const float3 worldPosition = gbuffer::ReconstructWorldPositionFromDepth(input.UV, depth, passData.InverseProjectionMatrix, passData.InverseViewMatrix);
    const float3 worldViewDirection = normalize(passData.WorldCameraPosition - worldPosition);

    const uint32_t outputType = BenzinGetRootConstant(g_OutputType);
    switch (outputType)
    {
        case OutputType::ReconsructedWorldPosition:
        {
            return float4(worldPosition, 1.0f);
        }
        case OutputType::GBuffer_Depth:
        {
            // TODO: 
            const float nearZ = 0.1f;
            const float farZ = 100.0f;
            const float linearDepth = (2.0 * nearZ) / (farZ + nearZ - depth * (farZ - nearZ));
            return float4(linearDepth.xxx, 1.0f);
        }
        case OutputType::GBuffer_Albedo:
        {
            return gbuffer.Albedo;
        }
        case OutputType::GBuffer_WorldNormal:
        {
            return float4(gbuffer.WorldNormal, 1.0f);
        }
        case OutputType::GBuffer_Emissive:
        {
            return float4(gbuffer.Emissive, 1.0f);
        }
        case OutputType::GBuffer_Roughness:
        {
            return float4(gbuffer.Roughness.xxx, 1.0f);
        }
        case OutputType::GBuffer_Metalness:
        {
            return float4(gbuffer.Metalness.xxx, 1.0f);
        }
        default:
        {
            break;
        }
    }

    pbr::Material material;
    material.Albedo = gbuffer.Albedo;
    material.Roughness = gbuffer.Roughness;
    material.Metalness = gbuffer.Metalness;
    material.F0 = pbr::GetF0(gbuffer.Albedo.rgb, gbuffer.Metalness);

    const float3 ambientColor = 0.1f * gbuffer.Albedo.rgb;

    float3 directColor = 0.0f;

    {
        DirectionalLight sunLight;
        sunLight.Color = passData.SunColor;
        sunLight.Intensity = passData.SunIntensity;
        sunLight.WorldDirection = passData.SunDirection;

        directColor += GetLitColorForDirectionalLight(sunLight, material, worldViewDirection, gbuffer.WorldNormal);
    }

    {
        for (uint32_t i = 0; i < passData.PointLightCount; ++i)
        {
            directColor += GetLitColorForPointLight(pointLightBuffer[i], material, worldPosition, worldViewDirection, gbuffer.WorldNormal);
        }
    }

    const float3 finalLitColor = ambientColor + directColor + gbuffer.Emissive;
    return float4(common::LinearToGamma(finalLitColor), 1.0f);
}
