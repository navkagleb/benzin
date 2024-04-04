#include "common.hlsli"

#include "bilinear_filter.hlsli"
#include "gbuffer.hlsli"

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

[numthreads(joint::tc::DenoiserTemporalAccumulation_X, joint::tc::DenoiserTemporalAccumulation_Y, joint::tc::DenoiserTemporalAccumulation_Z)]
void CsMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    ConstantBuffer<joint::FrameConstants> frameConstants = FetchFrameConstantBuffer();
    ConstantBuffer<joint::DoubleFrameCameraConstants> doubleCameraConstants = FetchDoubleFrameCameraConstants();

    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_WorldNormalTexture)];
    Texture2D<float> depthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_DepthBuffer)];
    Texture2D<float4> velocityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_VelocityBuffer)];
    Texture2D<float> previousViewDepthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_PreviousViewDepthBuffer)];
    Texture2D<float> previousTemporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_PreviousTemporalAccumulationBuffer)];

    RWTexture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserTemporalAccumulationRc_CurrentTemporalAccumulationBuffer)];

    if (any(dispatchThreadId.xy >= frameConstants.RenderResolution))
    {
        return;
    }

    const float2 uv = (dispatchThreadId.xy + 0.5) * frameConstants.InvRenderResolution;

    const float3 worldNormal = worldNormalTexture.SampleLevel(g_PointClampSampler, uv, 0).xyz;
    const float depth = depthBuffer.SampleLevel(g_PointClampSampler, uv, 0);
    const float3 motionVector = velocityBuffer.SampleLevel(g_PointClampSampler, uv, 0).xyz;

    const float2 previousUv = uv - motionVector.xy;
    const float previousDepth = depth - motionVector.z;
    const float3 previousViewPosition = ReconstructViewPositionFromDepth(previousUv, previousDepth, doubleCameraConstants.PreviousFrame.InverseProjection);
    const float3 previousWorldPosition = ReconstructWorldPositionFromViewPosition(previousViewPosition, doubleCameraConstants.PreviousFrame.InverseView);

    const BilinearFilter filterAtPreviousUv = CreateBilinearFilter(previousUv, frameConstants.RenderResolution);
    const float4 previousViewDepthSamples = GatherRedManually(previousViewDepthBuffer, filterAtPreviousUv);
    const float4 previousAccumulationCounts = GatherRedManually(previousTemporalAccumulationBuffer, filterAtPreviousUv);
 
    const bool4 isGatherOnScreen = bool4(filterAtPreviousUv.TopLeftTexelPosition >= 0.0, (filterAtPreviousUv.TopLeftTexelPosition + 1.0) < frameConstants.RenderResolution);
    const bool4 isOccludedByPlaneDistance = IsOccludedByPlaneDistance(worldNormal, previousWorldPosition, previousViewPosition, previousViewDepthSamples);
    const bool4 isGatherValid = isGatherOnScreen & !isOccludedByPlaneDistance;

    const float newAccumulationCount = ApplyBilinearCustomWeights(filterAtPreviousUv, min(previousAccumulationCounts + 1.0, frameConstants.MaxTemporalAccumulationCount), isGatherValid);

    const float accumulationCount = isGatherValid.x <= 0.0 ? 0.0 : newAccumulationCount;
    temporalAccumulationBuffer[dispatchThreadId.xy] = accumulationCount;
}
