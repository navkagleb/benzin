#include "common.hlsli"

#include "gbuffer.hlsli"

// Ref: https://wojtsterna.blogspot.com/2018/02/directx-11-hlsl-gatherred.html
// Ref: https://www.gdcvault.com/play/1026701/Fast-Denoising-With-Self-Stabilizing
// Ref: https://developer.nvidia.com/gtc/2020/video/s22699
// Ref: https://developer.download.nvidia.com/video/gputechconf/gtc/2020/presentations/s22699-fast-denoising-with-self-stabilizing-recurrent-blurs.pdf

struct BilinearFilter
{
    float2 TexelSize;
    float2 TopLeftTexelPosition;
    float2 Weights;
};

BilinearFilter CreateBilinearFilter(float2 uv, float2 textureSize)
{
    const float2 textureOffsetEpsilon = textureSize * 0.000001;
    const float2 texelPosition = (uv * textureSize) - 0.5 + textureOffsetEpsilon; // Force jump to the correct texel

    BilinearFilter filter;
    filter.TexelSize = 1.0 / textureSize;
    filter.TopLeftTexelPosition = floor(texelPosition);
    filter.Weights = frac(texelPosition);

    return filter;
};

float4 GatherRedManually(Texture2D<float> texture, BilinearFilter filter, float2 uv)
{
    // w z
    // x y
    // uv - points to 'w' texel
    // For gathering uv should point to 'y' texel

    const float2 gatherUv = (filter.TopLeftTexelPosition + 1.0) * filter.TexelSize; // Move to right bottom
    const float4 samples = texture.GatherRed(g_PointWithTransparentBlackBorderSampler, gatherUv).wzxy;

    return samples;
}

float4 ExpandBilinearWeights(BilinearFilter filter)
{
    const float2 weights = filter.Weights;

    // Expand 3 lerps into separate weights for each sample of the 2x2 footprint
    // lerp(
    //     lerp(w, z, weights.x),
    //     lerp(x, y, weights.x),
    //     weights.y
    // )

    float4 expandedWeights;
    expandedWeights.x = (1.0f - weights.x) * (1.0f - weights.y);
    expandedWeights.y = weights.x * (1.0f - weights.y);
    expandedWeights.z = (1.0f - weights.x) * weights.y;
    expandedWeights.w = weights.x * weights.y;

    return expandedWeights;
}

float ApplyBilinearCustomWeights(BilinearFilter filter, float4 gatheredValues, float4 customWeights)
{
    const float4 weights = ExpandBilinearWeights(filter) * customWeights;
    const float weightSum = dot(weights, 1.0);

    if (abs(weightSum) < 1.0e-5)
    {
        return 0.0;
    }

    const float gatheredSum = dot(gatheredValues * weights, 1.0);
    return gatheredSum * rcp(weightSum); // Normalization
}

static const float g_DisocclusionThresholdInPercentages = 0.05;

bool4 IsOccludedByPlaneDistance(float3 surfaceNormal, float3 previousWorldPosition, float3 previousViewPosition, float4 previousViewDepthSamples)
{
    const float distanceToPlane = abs(dot(surfaceNormal, previousWorldPosition));

    const float distanceToDepthRatio = distanceToPlane / previousViewPosition.z; // previousViewPosition.z always positive
    const float4 distancesToPlane = distanceToDepthRatio * previousViewDepthSamples;

    const float4 distancesDelta = abs(distancesToPlane - distanceToPlane);

    return step(g_DisocclusionThresholdInPercentages * distanceToPlane, distancesDelta);
}

[numthreads(8, 8, 1)]
void CS_Main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    ConstantBuffer<joint::FrameConstants> frameConstants = FetchFrameConstantBuffer();
    ConstantBuffer<joint::DoubleFrameCameraConstants> doubleCameraConstants = FetchDoubleFrameCameraConstants();

    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_WorldNormalTexture)];
    Texture2D<float> depthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_DepthBuffer)];
    Texture2D<float4> velocityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_VelocityBuffer)];
    Texture2D<float> previousViewDepthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_PreviousViewDepthBuffer)];
    Texture2D<float> previousTemporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_PreviousTemporalAccumulationBuffer)];
    Texture2D<float> previousVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_PreviousVisibilityBuffer)];
    Texture2D<float> currentVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_CurrentVisibilityBuffer)];

    RWTexture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_CurrentTemporalAccumulationBuffer)];
    RWTexture2D<float> denoisedVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::RtShadowDenoisingRc_DenoisedVisiblityBuffer)];

    if (any(dispatchThreadId.xy >= frameConstants.RenderResolution))
    {
        return;
    }

    const float2 uv = (dispatchThreadId.xy + 0.5) * frameConstants.InvRenderResolution;

    const float3 worldNormal = worldNormalTexture.SampleLevel(g_PointWrapSampler, uv, 0).xyz;
    const float depth = depthBuffer.SampleLevel(g_PointWrapSampler, uv, 0);
    const float3 motionVector = velocityBuffer.SampleLevel(g_PointWrapSampler, uv, 0).xyz;

    const float2 previousUv = uv - motionVector.xy;
    const float previousDepth = depth - motionVector.z;
    const float3 previousViewPosition = ReconstructViewPositionFromDepth(previousUv, previousDepth, doubleCameraConstants.PreviousFrame.InverseProjection);
    const float3 previousWorldPosition = ReconstructWorldPositionFromViewPosition(previousViewPosition, doubleCameraConstants.PreviousFrame.InverseView);

    const BilinearFilter filterAtPreviousUv = CreateBilinearFilter(previousUv, frameConstants.RenderResolution);
    const float4 previousViewDepthSamples = GatherRedManually(previousViewDepthBuffer, filterAtPreviousUv, previousUv);
    const float4 previousAccumulationCounts = GatherRedManually(previousTemporalAccumulationBuffer, filterAtPreviousUv, previousUv);
 
    const bool4 isGatherOnScreen = bool4(filterAtPreviousUv.TopLeftTexelPosition >= 0.0, (filterAtPreviousUv.TopLeftTexelPosition + 1.0) < frameConstants.RenderResolution);
    const bool4 isOccludedByPlaneDistance = IsOccludedByPlaneDistance(worldNormal, previousWorldPosition, previousViewPosition, previousViewDepthSamples);
    const bool4 isGatherValid = isGatherOnScreen & !isOccludedByPlaneDistance;

    const float newAccumulationCount = ApplyBilinearCustomWeights(filterAtPreviousUv, min(previousAccumulationCounts + 1.0, frameConstants.MaxTemporalAccumulationCount), isGatherValid);

    const float accumulationCount = isGatherValid.x <= 0.0 ? 0.0 : newAccumulationCount;
    temporalAccumulationBuffer[dispatchThreadId.xy] = accumulationCount;

    // ----------------

    const int2 previousPixelPosition = previousUv * frameConstants.RenderResolution;

    const bool isPreviousPixelInScreen = IsInRange(previousPixelPosition, (int2)0, (int2)frameConstants.RenderResolution);

    const float currentVisibility = currentVisibilityBuffer[dispatchThreadId.xy];
    const float previousVisibility = isPreviousPixelInScreen ? previousVisibilityBuffer[previousPixelPosition] : 0.0f;

    const float alpha = 0.3f; // Temporal fade, trading temporal stability for lag
    float filteredVisiblity = alpha * (1.0 - previousVisibility) + (1.0 - alpha) * currentVisibility;

    denoisedVisibilityBuffer[dispatchThreadId.xy] = filteredVisiblity;
}
