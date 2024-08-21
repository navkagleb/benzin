#define RenderPassConstantsType joint::DeferredLightingPassConstants
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "fullscreen_helper.hlsli"
#include "gbuffer.hlsli"
#include "pbr.hlsli"

struct DirectionalLight
{
    float3 Color;
    float Intensity;
    float3 WorldDirection;
};

float CalculateAttenuation(float distance, joint::PointLight pointLight)
{
    return 1.0f / (pointLight.ConstantAttenuation + pointLight.LinearAttenuation * distance + pointLight.ExponentialAttenuation * distance * distance);
}

float3 GetLitColorForDirectionalLight(DirectionalLight directionalLight, PbrMaterial material, float3 worldViewDirection, float3 worldNormal)
{
    PbrLight light;
    light.Color = directionalLight.Color;
    light.Intensity = directionalLight.Intensity;
    light.Direction = normalize(-directionalLight.WorldDirection);

    return GetPbrLitColor(light, material, worldViewDirection, worldNormal);
}

float3 GetLitColorForPointLight(joint::PointLight pointLight, PbrMaterial material, float3 worldPosition, float3 worldViewDirection, float3 worldNormal)
{
    float3 lightDirection = pointLight.WorldPosition - worldPosition;
    const float distance = length(lightDirection);

    lightDirection /= distance;

    PbrLight light;
    light.Color = pointLight.Color;
    light.Intensity = pointLight.Intensity;
    light.Direction = lightDirection;

    const float attenuation = CalculateAttenuation(distance, pointLight);
    const float3 pbr = GetPbrLitColor(light, material, worldViewDirection, worldNormal);

    return attenuation * pbr;
}

UnpackedGBuffer FetchGBuffer(float2 uv)
{
    Texture2D<float4> albedoAndRoughnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRc_AlbedoAndRoughnessTexture)];
    Texture2D<float4> emissiveAndMetallicTexture = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRc_EmissiveAndMetallicTexture)];
    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRc_WorldNormalTexture)];
    Texture2D<float2> velocityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRc_VelocityBuffer)];

    PackedGBuffer packedGBuffer;
    packedGBuffer.Color0 = albedoAndRoughnessTexture.SampleLevel(g_PointWrapSampler, uv, 0);
    packedGBuffer.Color1 = emissiveAndMetallicTexture.SampleLevel(g_PointWrapSampler, uv, 0);
    packedGBuffer.Color2 = worldNormalTexture.SampleLevel(g_PointWrapSampler, uv, 0);
    packedGBuffer.Color3 = float4(velocityBuffer.SampleLevel(g_PointWrapSampler, uv, 0), 0.0f, 0.0f);

    return UnpackGBuffer(packedGBuffer);
}

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    const float depth = FetchDepth(input.Uv, GetRootConstant(joint::DeferredLightingPassRc_DepthStencilTexture));
    if (depth == 1.0)
    {
        discard;
    }

    const UnpackedGBuffer gbuffer = FetchGBuffer(input.Uv);

    StructuredBuffer<joint::PointLight> pointLightBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRc_PointLightBuffer)];

    const joint::CameraConstants cameraConstants = g_FrameConstants.Camera;

    const float3 worldPosition = ReconstructWorldPositionFromDepth(input.Uv, depth, cameraConstants.InvViewToClip, cameraConstants.InvWorldToView);
    const float3 worldViewDirection = normalize(cameraConstants.WorldPosition - worldPosition);

    PbrMaterial material;
    material.Albedo = gbuffer.Albedo;
    material.Roughness = gbuffer.Roughness;
    material.Metallic = gbuffer.Metallic;
    material.F0 = GetF0(gbuffer.Albedo.rgb, gbuffer.Metallic);

    const float3 ambientColor = 0.3f * gbuffer.Albedo.rgb;

    float3 directColor = 0.0f;

    {
        DirectionalLight sunLight;
        sunLight.Color = g_PassConstants.SunColor;
        sunLight.Intensity = g_PassConstants.SunIntensity;
        sunLight.WorldDirection = g_PassConstants.SunDirection;

        directColor += GetLitColorForDirectionalLight(sunLight, material, worldViewDirection, gbuffer.WorldNormal);
    }

    {
        for (uint i = 0; i < g_PassConstants.ActivePointLightCount; ++i)
        {
            directColor += GetLitColorForPointLight(pointLightBuffer[i], material, worldPosition, worldViewDirection, gbuffer.WorldNormal);
        }
    }
    
    Texture2D<float> shadowVisiblityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRc_ShadowVisibilityBuffer)];
    const float shadowVisiblity = shadowVisiblityBuffer.Sample(g_LinearWrapSampler, input.Uv);

    const float3 finalLitColor = ambientColor + gbuffer.Emissive + directColor * (1.0 - shadowVisiblity);
    return float4(saturate(finalLitColor), 1.0f);
}
