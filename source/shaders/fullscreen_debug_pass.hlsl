#define RenderPassConstantsType joint::FullScreenDebugConstants
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "fullscreen_helper.hlsli"
#include "gbuffer.hlsli"
#include "space_convertions.hlsli"

float GetFloatByIndex(float4 values, uint index)
{
    switch (index)
    {
        case 0: return values.x;
        case 1: return values.y;
        case 2: return values.z;
        case 3: return values.w;
    }

    return g_NaN;
}

float4 PsMain(VsFullScreenTriangleOutput input) : SV_Target
{
    Texture2D<float4> albedoAndRoughnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_AlbedoAndRoughnessTexture)];
    Texture2D<float4> emissiveAndMetallicTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_EmissiveAndMetallicTexture)];
    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_WorldNormalTexture)];
    Texture2D<float2> velocityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_VelocityBuffer)];
    Texture2D<float> viewDepthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_ViewDepthBuffer)];
    Texture2D<float> depthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_DepthBuffer)];

    Texture2D<float> shadowVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_ShadowVisibilityBuffer)];
    Texture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_TemporalAccumulationBuffer)];
    Texture2D<float> reprojectedHistoryTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_ReprojectedHistoryTexture)];
    Texture2D<float> denoisedShadowVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_DenoisedShadowVisibilityBuffer)];

    Texture2D<float4> sigmaTiles = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_SigmaTiles)];
    Texture2D<float2> sigmaSmoothTiles = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_SigmaSmoothTiles)];

    PackedGBuffer packedGBuffer;
    packedGBuffer.Color0 = albedoAndRoughnessTexture.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color1 = emissiveAndMetallicTexture.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color2 = worldNormalTexture.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
    packedGBuffer.Color3 = float4(velocityBuffer.SampleLevel(g_PointClampSampler, input.Uv, 0.0), 0.0, 0.0);

    UnpackedGBuffer gbuffer = UnpackGBuffer(packedGBuffer);

    const float depth = depthBuffer.SampleLevel(g_PointClampSampler, input.Uv, 0);

    // if (depth == 1.0f)
    // {
    //     discard;
    // }

    switch (g_PassConstants.OutputType)
    {
        case joint::DebugOutputType_ReconsructedWorldPosition:
        {
            const joint::CameraConstants cameraConstants = g_FrameConstants.Camera;
            const float3 worldPosition = ReconstructWorldPositionFromDepth(input.Uv, depth, cameraConstants.InvViewToClip, cameraConstants.InvWorldToView);
            
#if 1
            const float3 viewPosition = ReconstructViewPositionFromDepth(input.Uv, depth, cameraConstants.InvViewToClip);
            const float4 clipPosition = mul(float4(viewPosition, 1.0), cameraConstants.ViewToClip);
            const float3 ndcPosition = ClipPositionToNdcPosition(clipPosition);
            const float2 uv = NdcPositionToUv(ndcPosition);
            
            return float4(uv, 0.0, 1.0);
#endif
            return float4(worldPosition, 1.0);
        }
        case joint::DebugOutputType_GBufferAlbedo: return float4(gbuffer.Albedo, 1.0);
        case joint::DebugOutputType_GBufferRoughness: return float4(gbuffer.Roughness.xxx, 1.0);
        case joint::DebugOutputType_GBufferEmissive: return float4(gbuffer.Emissive, 1.0);
        case joint::DebugOutputType_GBufferMetallic: return float4(gbuffer.Metallic.xxx, 1.0);
        case joint::DebugOutputType_GBufferWorldNormal: return float4(gbuffer.WorldNormal, 1.0);
        // case joint::DebugOutputType_GBufferVelocityBuffer: return float4(gbuffer.MotionVector, 0.0, 1.0f); #TODO
        case joint::DebugOutputType_GBufferViewDepthBuffer:
        {
            const float viewDepth = viewDepthBuffer.SampleLevel(g_PointClampSampler, input.Uv, g_PassConstants.ViewDepthMipIndex);
            return float4((viewDepth + g_PassConstants.MinViewDepth) / g_PassConstants.MaxViewDepth, 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_CurrentShadowVisibility:
        {
            const float shadowVisibility = shadowVisibilityBuffer.SampleLevel(g_PointClampSampler, input.Uv, g_PassConstants.ViewDepthMipIndex);
            return float4(shadowVisibility, 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_TemporalAccumulationBuffer:
        {
            // Grab by pixel position due to R32 texture doesn't support sampling

            const uint2 texelPosition = input.Uv * g_FrameConstants.RenderResolution;
            const float temporalAccumulation = temporalAccumulationBuffer[texelPosition];

            return temporalAccumulation / g_FrameConstants.MaxTemporalAccumulationCount;
        }
        case joint::DebugOutputType_ReprojectedHistory:
        {
            const float reprojectedVisibilitySample = reprojectedHistoryTexture.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(reprojectedVisibilitySample, 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_DenoisedShadowVisibilityBuffer:
        {
            const float sample = denoisedShadowVisibilityBuffer.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(sample, 0.0, 0.0, 1.0);
        }
        case joint::DebugOutputType_SigmaTiles:
        {
            const float3 sample = sigmaTiles.SampleLevel(g_PointClampSampler, input.Uv, 0.0).xyz;
            return float4(sample, 1.0);
        }
        case joint::DebugOutputType_SigmaSmoothTiles:
        {
            const float2 sample = sigmaSmoothTiles.SampleLevel(g_PointClampSampler, input.Uv, 0.0);
            return float4(sample, 0.0, 1.0);
        }
    }

    return float4(1.0, 0.0, 1.0, 1.0);
}
