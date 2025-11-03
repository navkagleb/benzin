#include "joint/deferred_lighting_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "gbuffer.hlsli"
#include "pbr.hlsli"
#include "sigma_denoiser/sigma_public.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_AlbedoAndRoughness, joint::DeferredLightingResources::AlbedoAndRoughness);
BenzinDeclareRootResource(Texture2D<float4>, g_EmissiveAndMetallic, joint::DeferredLightingResources::EmissiveAndMetallic);
BenzinDeclareRootResource(Texture2D<float4>, g_WorldNormal, joint::DeferredLightingResources::WorldNormal);
BenzinDeclareRootResource(Texture2D<float>, g_Depth, joint::DeferredLightingResources::DepthStencil);
BenzinDeclareRootResource(Texture2D<float>, g_Shadow, joint::DeferredLightingResources::Shadow);

float3 CalcSunLight(PbrMaterial material, float3 worldToEyeDir, float3 worldNormal)
{
    PbrLight light;
    light.Color = g_SunLightConsts.Color;
    light.Intensity = g_SunLightConsts.Intensity;
    light.Direction = g_SunLightConsts.WorldPosition;

    return GetPbrLitColor(light, material, worldToEyeDir, worldNormal);
}

GBuffer FetchGBuffer(uint2 pixelIndex)
{
    PackedGBuffer packedGBuffer = (PackedGBuffer)0;
    packedGBuffer.Color0 = g_AlbedoAndRoughness[pixelIndex];
    packedGBuffer.Color1 = g_EmissiveAndMetallic[pixelIndex];
    packedGBuffer.Color2 = g_WorldNormal[pixelIndex];

    return UnpackGBuffer(packedGBuffer);
}

struct VsOutput
{
    float4 m_SvPosition : SV_Position;
    float2 m_Uv : Uv;
};

VsOutput VsMain(uint vertexIndex : SV_VertexID)
{
    VsOutput output = (VsOutput)0;
    output.m_SvPosition = GetFullScreenTriangleClipPosition(vertexIndex);
    output.m_Uv = GetFullScreenTriangleUv(vertexIndex);

    return output;
}

float4 PsMain(VsOutput input) : SV_Target
{
    const uint2 pixelIndex = input.m_SvPosition.xy;
    const GBuffer gbuffer = FetchGBuffer(pixelIndex);

    const float depth = g_Depth[pixelIndex];
    const float3 worldPosition = ReconstructWorldPosition(input.m_Uv, depth, GetCameraConsts().ClipToView, GetCameraConsts().ViewToWorld);
    const float3 worldToEyeDir = normalize(GetCameraConsts().WorldPosition - worldPosition);

    PbrMaterial material;
    material.Albedo = gbuffer.Albedo;
    material.Roughness = gbuffer.Roughness;
    material.Metallic = gbuffer.Metallic;
    material.F0 = GetF0(gbuffer.Albedo.rgb, gbuffer.Metallic);

    float sunShadowFactor = g_Shadow[pixelIndex];
    sunShadowFactor = g_FrameConsts.IsDenoiserEnabled ? sigma::UnpackShadow(sunShadowFactor) : sigma::IsLit(sunShadowFactor);

    float3 sunLight = CalcSunLight(material, worldToEyeDir, gbuffer.WorldNormal);
    sunLight *= sunShadowFactor;

    const float3 ambientColor = 0.2 * gbuffer.Albedo.rgb;
    const float3 finalColor = ambientColor + sunLight + gbuffer.Emissive;

    return float4(finalColor, 1.0);
}
