#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "bilinear_filter.hlsli"
#include "gbuffer.hlsli"
#include "space_convertions.hlsli"

// Ref: https://www.gdcvault.com/play/1026701/Fast-Denoising-With-Self-Stabilizing
// Ref: https://developer.nvidia.com/gtc/2020/video/s22699
// Ref: https://developer.download.nvidia.com/video/gputechconf/gtc/2020/presentations/s22699-fast-denoising-with-self-stabilizing-recurrent-blurs.pdf
// Ref: RayTracing Gems 2. CHAPTER 49. REBLUR: A HIERARCHICAL RECURRENT DENOISER

static const float g_DisocclusionThresholdInPercentages = 0.05;

bool4 IsOccludedByPlaneDistance(float3 surfaceNormal, float3 previousWorldPosition, float3 previousViewPosition, float4 previousViewDepthSamples)
{
    const float distanceToPlane = abs(dot(surfaceNormal, previousWorldPosition));

    const float distanceToDepthRatio = distanceToPlane / previousViewPosition.z; // previousViewPosition.z always positive
    const float4 distancesToPlane = distanceToDepthRatio * previousViewDepthSamples;

    const float4 distancesDelta = abs(distancesToPlane - distanceToPlane);

    return step(g_DisocclusionThresholdInPercentages * distanceToPlane, distancesDelta);
}

[numthreads(joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z)]
void CsMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_WorldNormalTexture)];
    Texture2D<float> depthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_DepthBuffer)];
    Texture2D<float4> velocityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_VelocityBuffer)];
    Texture2D<float> previousViewDepthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_PreviousViewDepthBuffer)];
    Texture2D<float> previousTemporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_PreviousTemporalAccumulationBuffer)];
    Texture2D<float> previousDenoisedVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_PreviousDenoisedVisibilityBuffer)];

    RWTexture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_TemporalAccumulationBuffer)];
    RWTexture2D<float> reprojectedHistoryTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_ReprojectedHistoryTexture)];

    if (any(dispatchThreadId.xy >= g_FrameConstants.RenderResolution))
    {
        return;
    }

    const joint::CameraConstants previousCameraConstants = g_FrameConstants.PreviousCamera;

    const float2 uv = DispatchThreadIdToUv(dispatchThreadId, g_FrameConstants.InvRenderResolution);

    const float3 worldNormal = worldNormalTexture.SampleLevel(g_PointClampSampler, uv, 0).xyz;
    const float depth = depthBuffer.SampleLevel(g_PointClampSampler, uv, 0);
    const float3 motionVector = velocityBuffer.SampleLevel(g_PointClampSampler, uv, 0).xyz;

    const float2 previousUv = uv - motionVector.xy;
    const float previousDepth = depth - motionVector.z;
    const float3 previousViewPosition = ReconstructViewPositionFromDepth(previousUv, previousDepth, previousCameraConstants.InverseProjection);
    const float3 previousWorldPosition = ReconstructWorldPositionFromViewPosition(previousViewPosition, previousCameraConstants.InverseView);

    const BilinearFilter filterAtPreviousUv = CreateBilinearFilter(previousUv, g_FrameConstants.RenderResolution);
    const float4 previousViewDepthSamples = GatherRedManually(previousViewDepthBuffer, filterAtPreviousUv);
    const float4 previousAccumulationCounts = GatherRedManually(previousTemporalAccumulationBuffer, filterAtPreviousUv);
 
    const bool4 isGatherOnScreen = bool4(filterAtPreviousUv.TopLeftTexelPosition >= 0.0, (filterAtPreviousUv.TopLeftTexelPosition + 1.0) < g_FrameConstants.RenderResolution);
    const bool4 isOccludedByPlaneDistance = IsOccludedByPlaneDistance(worldNormal, previousWorldPosition, previousViewPosition, previousViewDepthSamples);
    const bool4 isGatherValid = isGatherOnScreen & !isOccludedByPlaneDistance;
    
    {
        const float4 accumulationCounts = min(previousAccumulationCounts + 1.0, g_FrameConstants.MaxTemporalAccumulationCount);
        float accumulationCount = ApplyBilinearCustomWeights(filterAtPreviousUv, accumulationCounts, isGatherValid);
        accumulationCount = isGatherValid.x <= 0.0 ? 0.0 : accumulationCount; // Check only texel in write location

        temporalAccumulationBuffer[dispatchThreadId.xy] = accumulationCount;
    }

    {
        const float4 previousDenoisedVisibilitySamples = GatherRedManually(previousDenoisedVisibilityBuffer, filterAtPreviousUv);
        float reprojectedHistorySample = ApplyBilinearCustomWeights(filterAtPreviousUv, previousDenoisedVisibilitySamples, isGatherValid);
        reprojectedHistorySample = isGatherValid.x <= 0.0 ? 0.0 : reprojectedHistorySample; // Check only texel in write location

        // For the first frame the 'previousDenoisedVisibilityBuffer' contains garbage (-NaN)
        // So saturate it and -NaN go to 0 which means no shadow
        reprojectedHistoryTexture[dispatchThreadId.xy] = !isnan(reprojectedHistorySample) ? reprojectedHistorySample : 0.0;
    }
}
