#include "common.hlsli"

#include "gbuffer.hlsli"
#include "fullscreen_helper.hlsli"

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

float4 PS_Main(VS_FullScreenTriangleOutput input) : SV_Target
{
    ConstantBuffer<joint::FrameConstants> frameConstants = FetchFrameConstantBuffer();
    ConstantBuffer<joint::DoubleFrameCameraConstants> doubleCameraConstants = ResourceDescriptorHeap[GetRootConstant(joint::GlobalRc_CameraConstantBuffer)];
    ConstantBuffer<joint::FullScreenDebugConstants> passConstants = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_PassConstantBuffer)];

    Texture2D<float4> albedoAndRoughnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_AlbedoAndRoughnessTexture)];
    Texture2D<float4> emissiveAndMetallicTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_EmissiveAndMetallicTexture)];
    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_WorldNormalTexture)];
    Texture2D<float2> velocityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_VelocityBuffer)];
    Texture2D<float> depthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_DepthBuffer)];

    Texture2D<float> shadowVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_ShadowVisibilityBuffer)];
    Texture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::FullScreenDebugRc_TemporalAccumulationBuffer)];

#if 1
    PackedGBuffer packedGBuffer;
    packedGBuffer.Color0 = albedoAndRoughnessTexture.SampleLevel(g_PointWrapSampler, input.UV, 0);
    packedGBuffer.Color1 = emissiveAndMetallicTexture.SampleLevel(g_PointWrapSampler, input.UV, 0);
    packedGBuffer.Color2 = worldNormalTexture.SampleLevel(g_PointWrapSampler, input.UV, 0);
    packedGBuffer.Color3 = float4(velocityBuffer.SampleLevel(g_PointWrapSampler, input.UV, 0), 0.0f, 0.0f);

    UnpackedGBuffer gbuffer = UnpackGBuffer(packedGBuffer);
#endif

    const float depth = depthBuffer.SampleLevel(g_PointWrapSampler, input.UV, 0);

    if (depth == 1.0f)
    {
        discard;
    }

    switch (passConstants.OutputType)
    {
        case joint::DebugOutputType_ReconsructedWorldPosition:
        {
            const joint::CameraConstants cameraConstants = doubleCameraConstants.CurrentFrame;
            const float3 worldPosition = ReconstructWorldPositionFromDepth(input.UV, depth, cameraConstants.InverseProjection, cameraConstants.InverseView);
            
            return float4(worldPosition, 1.0f);
        }
#if 1
        case joint::DebugOutputType_GBufferAlbedo: return float4(gbuffer.Albedo, 1.0f);
        case joint::DebugOutputType_GBufferRoughness: return float4(gbuffer.Roughness.xxx, 1.0f);
        case joint::DebugOutputType_GBufferEmissive: return float4(gbuffer.Emissive, 1.0f);
        case joint::DebugOutputType_GBufferMetallic: return float4(gbuffer.Metallic.xxx, 1.0f);
        case joint::DebugOutputType_GBufferWorldNormal: return float4(gbuffer.WorldNormal, 1.0f);
        // case joint::DebugOutputType_GBufferVelocityBuffer: return float4(gbuffer.MotionVector, 0.0, 1.0f); #TODO
#endif
        case joint::DebugOutputType_CurrentShadowVisibility:
        {
            const float shadowVisibility = shadowVisibilityBuffer.SampleLevel(g_PointWrapSampler, input.UV, 0);

            return float4(shadowVisibility, 0.0f, 0.0f, 1.0f);
        }
        case joint::DebugOutputType_TemporalAccumulationBuffer:
        {
            // Grab by pixel position due to R32 texture doesn't support sampling

            const uint2 texelPosition = input.UV * frameConstants.RenderResolution;
            const float temporalAccumulation = temporalAccumulationBuffer[texelPosition];

            return temporalAccumulation / frameConstants.MaxTemporalAccumulationCount;
        }
    }

    return float4(1.0f, 0.0f, 1.0f, 1.0f);
}
