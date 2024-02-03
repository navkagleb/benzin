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

float3 GetLitColorForDirectionalLight(DirectionalLight directionalLight, PBRMaterial material, float3 worldViewDirection, float3 worldNormal)
{
    PBRLight light;
    light.Color = directionalLight.Color;
    light.Intensity = directionalLight.Intensity;
    light.Direction = normalize(-directionalLight.WorldDirection);

    return GetPBRLitColor(light, material, worldViewDirection, worldNormal);
}

float3 GetLitColorForPointLight(joint::PointLight pointLight, PBRMaterial material, float3 worldPosition, float3 worldViewDirection, float3 worldNormal)
{
    float3 lightDirection = pointLight.WorldPosition - worldPosition;
    const float distance = length(lightDirection);

    lightDirection /= distance;

    PBRLight light;
    light.Color = pointLight.Color;
    light.Intensity = pointLight.Intensity;
    light.Direction = lightDirection;

    const float attenuation = CalculateAttenuation(distance, pointLight);
    const float3 pbr = GetPBRLitColor(light, material, worldViewDirection, worldNormal);

    return attenuation * pbr;
}

float FetchDepth(float2 uv)
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRC_DepthStencilTexture)];
    return depthTexture.Sample(g_LinearWrapSampler, uv).r;
}

UnpackedGBuffer FetchGBuffer(float2 uv)
{
    Texture2D<float4> albedoTexture = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRC_AlbedoTexture)];
    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRC_WorldNormalTexture)];
    Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRC_EmissiveTexture)];
    Texture2D<float4> roughnessMetalnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRC_RoughnessMetalnessTexture)];

    PackedGBuffer packedGBuffer;
    packedGBuffer.Color0 = albedoTexture.Sample(g_LinearWrapSampler, uv);
    packedGBuffer.Color1 = worldNormalTexture.Sample(g_LinearWrapSampler, uv);
    packedGBuffer.Color2 = emissiveTexture.Sample(g_LinearWrapSampler, uv);
    packedGBuffer.Color3 = roughnessMetalnessTexture.Sample(g_LinearWrapSampler, uv);

    return UnpackGBuffer(packedGBuffer);
}

joint::DeferredLightingPassConstants FetchPassConstants()
{
    ConstantBuffer<joint::DeferredLightingPassConstants> passConstants = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRC_PassConstantBuffer)];
    return passConstants;
}

float4 PS_Main(VS_FullScreenTriangleOutput input) : SV_Target
{
    const float depth = FetchDepth(input.UV);
    if (depth == 1.0f)
    {
        discard;
    }

    const joint::CameraConstants cameraConstants = FetchCameraConstants();
    const joint::DeferredLightingPassConstants passConstants = FetchPassConstants();
    const UnpackedGBuffer gbuffer = FetchGBuffer(input.UV);

    StructuredBuffer<joint::PointLight> pointLightBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DeferredLightingPassRC_PointLightBuffer)];

    const float3 worldPosition = ReconstructWorldPositionFromDepth(input.UV, depth, cameraConstants.InverseProjection, cameraConstants.InverseView);
    const float3 worldViewDirection = normalize(cameraConstants.WorldPosition - worldPosition);

    switch (passConstants.OutputType)
    {
        case joint::DeferredLightingOutputType_ReconsructedWorldPosition:
        {
            return float4(worldPosition, 1.0f);
        }
        case joint::DeferredLightingOutputType_GBuffer_Depth:
        {
            // TODO: 
            const float nearZ = 0.1f;
            const float farZ = 1000.0f;
            const float linearDepth = (2.0 * nearZ) / (farZ + nearZ - depth * (farZ - nearZ));
            return float4(linearDepth.xxx, 1.0f);
        }
        case joint::DeferredLightingOutputType_GBuffer_Albedo:
        {
            return gbuffer.Albedo;
        }
        case joint::DeferredLightingOutputType_GBuffer_WorldNormal:
        {
            return float4(gbuffer.WorldNormal, 1.0f);
        }
        case joint::DeferredLightingOutputType_GBuffer_Emissive:
        {
            return float4(gbuffer.Emissive, 1.0f);
        }
        case joint::DeferredLightingOutputType_GBuffer_Roughness:
        {
            return float4(gbuffer.Roughness.xxx, 1.0f);
        }
        case joint::DeferredLightingOutputType_GBuffer_Metalness:
        {
            return float4(gbuffer.Metalness.xxx, 1.0f);
        }
        default:
        {
            break;
        }
    }

    PBRMaterial material;
    material.Albedo = gbuffer.Albedo;
    material.Roughness = gbuffer.Roughness;
    material.Metalness = gbuffer.Metalness;
    material.F0 = GetF0(gbuffer.Albedo.rgb, gbuffer.Metalness);

    const float3 ambientColor = 0.1f * gbuffer.Albedo.rgb;

    float3 directColor = 0.0f;

    {
        DirectionalLight sunLight;
        sunLight.Color = passConstants.SunColor;
        sunLight.Intensity = passConstants.SunIntensity;
        sunLight.WorldDirection = passConstants.SunDirection;

        directColor += GetLitColorForDirectionalLight(sunLight, material, worldViewDirection, gbuffer.WorldNormal);
    }

    {
        for (uint i = 0; i < passConstants.ActivePointLightCount; ++i)
        {
            directColor += GetLitColorForPointLight(pointLightBuffer[i], material, worldPosition, worldViewDirection, gbuffer.WorldNormal);
        }
    }
    
    const float3 finalLitColor = ambientColor + directColor + gbuffer.Emissive;
    return float4(saturate(finalLitColor), 1.0f);
}
