#define RenderPassConstantsType joint::DeferredLightingPassConstants
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "fullscreen_helper.hlsli"
#include "gbuffer.hlsli"
#include "pbr.hlsli"
#include "sigma_denoiser/sigma_public.hlsli"
#include "space_convertions.hlsli"

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
    light.Direction = directionalLight.WorldDirection; // Assume that directionalLight.WorldDirection is directed towards the sun

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

BenzinDeclareRootResource(Texture2D<float4>, g_AlbedoAndRoughnessTex, joint::DeferredLightingPassRc_AlbedoAndRoughnessTex);
BenzinDeclareRootResource(Texture2D<float4>, g_EmissiveAndMetallicTex, joint::DeferredLightingPassRc_EmissiveAndMetallicTex);
BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormalTex, joint::DeferredLightingPassRc_WorldNormalTex);
BenzinDeclareRootResource(Texture2D<float2>, g_VelocityTex, joint::DeferredLightingPassRc_VelocityTex);
BenzinDeclareRootResource(Texture2D<float>, g_DepthTex, joint::DeferredLightingPassRc_DepthStencilTex);
BenzinDeclareRootResource(StructuredBuffer<joint::PointLight>, g_PointLightBuf, joint::DeferredLightingPassRc_PointLightBuf);
BenzinDeclareRootResource(Texture2D<float>, g_SigmaShadowTex, joint::DeferredLightingPassRc_SigmaShadowTex);

UnpackedGBuffer FetchGBuffer(float2 uv)
{
    PackedGBuffer packedGBuffer;
    packedGBuffer.Color0 = g_AlbedoAndRoughnessTex.SampleLevel(g_PointClampSampler, uv, 0.0);
    packedGBuffer.Color1 = g_EmissiveAndMetallicTex.SampleLevel(g_PointClampSampler, uv, 0.0);
    packedGBuffer.Color2 = g_WorldNormalTex.SampleLevel(g_PointClampSampler, uv, 0.0);
    packedGBuffer.Color3 = float4(g_VelocityTex.SampleLevel(g_PointClampSampler, uv, 0.0), 0.0, 0.0);

    return UnpackGBuffer(packedGBuffer);
}

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    const float depth = g_DepthTex.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    if (depth == 1.0)
    {
        discard;
    }

    const UnpackedGBuffer gbuffer = FetchGBuffer(input.Uv);

    const joint::CameraConstants cameraConstants = g_FrameConstants.Camera;

    const float3 worldPosition = ReconstructWorldPosition(input.Uv, depth, cameraConstants.ClipToView, cameraConstants.ViewToWorld);
    const float3 worldViewDirection = normalize(cameraConstants.WorldPosition - worldPosition);

    PbrMaterial material;
    material.Albedo = gbuffer.Albedo;
    material.Roughness = gbuffer.Roughness;
    material.Metallic = gbuffer.Metallic;
    material.F0 = GetF0(gbuffer.Albedo.rgb, gbuffer.Metallic);

    const float3 ambientColor = 0.3 * gbuffer.Albedo.rgb;

    float3 directColor = 0.0;

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
            directColor += GetLitColorForPointLight(g_PointLightBuf[i], material, worldPosition, worldViewDirection, gbuffer.WorldNormal);
        }
    }

    float shadowFactor = g_SigmaShadowTex.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    shadowFactor = g_FrameConstants.IsDenoiserEnabled ? sigma::UnpackShadow(shadowFactor) : sigma::IsLit(shadowFactor);

    const float3 finalLitColor = ambientColor + gbuffer.Emissive + directColor * shadowFactor;
    return float4(saturate(finalLitColor), 1.0f);
}
