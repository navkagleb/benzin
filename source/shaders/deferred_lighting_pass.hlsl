#include "joint/deferred_lighting_resources.hpp"
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

float3 GetLitColorForSun(joint::Light light, PbrMaterial material, float3 worldViewDirection, float3 worldNormal)
{
    PbrLight _light;
    _light.Color = light.Color;
    _light.Intensity = light.Intensity;
    _light.Direction = light.WorldPosition;

    return GetPbrLitColor(_light, material, worldViewDirection, worldNormal);
}

float CalculateAttenuationFactor(float distanceToLight, float3 attenuation)
{
    float attenuationFactor = attenuation.x;
    attenuationFactor += attenuation.y * distanceToLight;
    attenuationFactor += attenuation.z * distanceToLight * distanceToLight;
    attenuationFactor = 1.0 / attenuationFactor;

    return attenuationFactor;
}

float3 GetLitColor(joint::Light light, PbrMaterial material, float3 worldPosition, float3 worldViewDirection, float3 worldNormal)
{
    PbrLight _light;
    _light.Color = light.Color;

    switch (light.Type)
    {
        case joint::LightType::Sun:
        {
            _light.Intensity = light.Intensity;
            _light.Direction = light.WorldPosition;

            break;
        }
        case joint::LightType::Spherical:
        {
            float3 toLightDirection = light.WorldPosition - worldPosition;
            const float distanceToLight = length(toLightDirection);

            toLightDirection /= distanceToLight;

            _light.Intensity = light.Intensity * CalculateAttenuationFactor(distanceToLight, light.Attenuation);
            _light.Direction = toLightDirection;

            break;
        }
    }

    return GetPbrLitColor(_light, material, worldViewDirection, worldNormal);
}

BenzinDeclareRootResource(Texture2D<float4>, g_AlbedoAndRoughness, joint::Rc_DeferredLighting::AlbedoAndRoughness);
BenzinDeclareRootResource(Texture2D<float4>, g_EmissiveAndMetallic, joint::Rc_DeferredLighting::EmissiveAndMetallic);
BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormal, joint::Rc_DeferredLighting::WorldNormal);
BenzinDeclareRootResource(Texture2D<float>, g_Depth, joint::Rc_DeferredLighting::DepthStencil);
BenzinDeclareRootResource(Texture2DArray<float>, g_Shadow, joint::Rc_DeferredLighting::Shadow);

UnpackedGBuffer FetchGBuffer(float2 uv)
{
    PackedGBuffer packedGBuffer = (PackedGBuffer)0;
    packedGBuffer.Color0 = g_AlbedoAndRoughness.SampleLevel(g_PointClampSampler, uv, 0.0);
    packedGBuffer.Color1 = g_EmissiveAndMetallic.SampleLevel(g_PointClampSampler, uv, 0.0);
    packedGBuffer.Color2 = g_WorldNormal.SampleLevel(g_PointClampSampler, uv, 0.0);

    return UnpackGBuffer(packedGBuffer);
}

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    const float depth = g_Depth.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    if (depth == 1.0)
    {
        discard;
    }

    const UnpackedGBuffer gbuffer = FetchGBuffer(input.Uv);

    const joint::CameraConsts cameraConstants = g_FrameConstants.Camera;

    const float3 worldPosition = ReconstructWorldPosition(input.Uv, depth, cameraConstants.ClipToView, cameraConstants.ViewToWorld);
    const float3 worldViewDirection = normalize(cameraConstants.WorldPosition - worldPosition);

    PbrMaterial material;
    material.Albedo = gbuffer.Albedo;
    material.Roughness = gbuffer.Roughness;
    material.Metallic = gbuffer.Metallic;
    material.F0 = GetF0(gbuffer.Albedo.rgb, gbuffer.Metallic);

    const float3 ambientColor = 0.3 * gbuffer.Albedo.rgb;

    float3 directColor = 0.0;

    [unroll(4)]
    for (uint i = 0; i < g_FrameConstants.LightCount; ++i)
    {
        // float shadowFactor = g_Shadow.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
        float shadowFactor = g_Shadow[uint3(input.SvPosition.xy, i)];
        shadowFactor = g_FrameConstants.IsDenoiserEnabled ? sigma::UnpackShadow(shadowFactor) : sigma::IsLit(shadowFactor);

        float3 litColorFromLight = GetLitColor(g_Lights[i], material, worldPosition, worldViewDirection, gbuffer.WorldNormal);
        litColorFromLight *= shadowFactor;

        directColor += litColorFromLight;
    }

    const float3 finalLitColor = ambientColor + gbuffer.Emissive + directColor;
    return float4(finalLitColor, 1.0);
}
