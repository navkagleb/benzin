#define RenderPassConstantsType joint::DenoiserBlurConstants
#include "unified_root_parameters.hlsli"

#include "common.hlsli"
#include "gbuffer.hlsli"
#include "space_convertions.hlsli"

// PoissonDiskSampling. Ref: https://github.com/bartwronski/PoissonSamplingGenerator

static const uint g_PoissonSampleCount = 8;
static const float2 g_PoissonSamples[g_PoissonSampleCount] =
{
    //float2(-0.15351850522240756, 0.9223711101304518),
    //float2(0.24601660796620223, -0.9437090551851363),
    //float2(0.9582739764039027, 0.2520038077215424),
    //float2(-0.9344538063037932, -0.276470569050182),
    //float2(0.13486568259189927, -0.051045348436304226),
    //float2(0.8261934736549518, -0.5602379572446372),
    //float2(-0.47794000818564897, -0.8461013227046816),
    //float2(-0.5556170944581258, 0.3292488304636304),

    float2(0.0, 0.0),
    float2(0.18209281290245863, 0.9820576665834655),
    float2(-0.977793822632953, 0.03291455549247062),
    float2(0.9763490369791359, -0.1962402472856595),
    float2(-0.3923259236977481, -0.9181980582231567),
    float2(0.4791105476210399, -0.824867151160824),
    float2(-0.5687852599748795, 0.6528825513375083),
    float2(0.6080788867056116, 0.42260358791146085),
};

float CalcParallax(float3 worldPosition, float3 previousWorldPosition)
{
    const joint::CameraConstants cameraConstants = g_FrameConstants.Camera;
    const joint::CameraConstants prevCameraConstants = g_FrameConstants.PrevCamera;

    const float3 worldCameraDelta = cameraConstants.WorldPosition - prevCameraConstants.WorldPosition;
    const float3 worldMovementDelta = worldPosition - (previousWorldPosition - worldCameraDelta);

    const float distanceToPoint = distance(prevCameraConstants.WorldPosition, previousWorldPosition);

    // ~sine of angle between old and new view vector in world space
    // Measure of relative surface-camera motion.
    const float parallax = length(worldMovementDelta) / (distanceToPoint * g_FrameConstants.FrameTimeInSec);

    return parallax;
}

float GetMaxAllowedAccumulatedFrameCountUsingSurfaceMotion(float roughness, float nDotL, float parallax)
{
    float acos01sq = saturate(1.0 - nDotL);
    float a = pow(acos01sq, g_PassConstants.SpecularAccumulationCurve);
    float b = 1.001 + roughness * roughness;
    float angularSensitivity = (b + a) / (b - a);
    float power = g_PassConstants.SpecularAccumulationBasePower * (1.0 + parallax * angularSensitivity);

    return g_FrameConstants.MaxTemporalAccumulationCount * pow(roughness, power);
}

float3 GetGgxDominantDirection(float3 viewNormal, float3 viewDirection, float roughness)
{
    // Page69. Ref: https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
    // GgxDominantDirection - it's a direction where waste amount of energy is coming from ??

    const float3 reflectDirection = reflect(-viewDirection, viewNormal);
    const float smoothness = saturate(1.0 - roughness);
    const float lerpFactor = smoothness * (sqrt(smoothness) + roughness);

    const float3 dominantDirection = lerp(viewNormal, reflectDirection, lerpFactor);
    return normalize(dominantDirection);
}

float GetGgxSpecularLobeHalfAngleInRadians(float roughness)
{
    const float roughness2 = roughness * roughness;
    const float angleInDegrees = (90.0 * roughness2) / (1.0 + roughness2);

    return DegreesToRadians(angleInDegrees);
}

float3x3 GetKernelBasis(float3 viewPosition, float3 viewNormal, float roughness, float blurRadius, float normalizedAccumulatedFrameCount)
{
    const float3 viewDirection = -normalize(viewPosition);
    const float3 dominantDirection = GetGgxDominantDirection(viewNormal, viewDirection, roughness);
    const float3 reflectedDominantDirection = reflect(-dominantDirection, viewNormal);

    const float3 tangent = normalize(cross(viewNormal, reflectedDominantDirection)); // #TODO: Handle the case when viewNormal = reflectedDominantDirection
    const float3 bitangent = cross(reflectedDominantDirection, tangent);

    // Anisotropic sampling
    // Tangent gets scaled more under glandcing angles
    // Skew factor depends on roughness
    // If accumulation goes badly kernel shape moves towards isotropic (in the world space, still anisotropic in the screen space)
    const float angle = saturate(acos(viewNormal.z) / g_PiDiv2);
    const float skewFactor = lerp(1.0, roughness, angle);

    return float3x3(
        tangent * blurRadius * lerp(1.0, skewFactor, normalizedAccumulatedFrameCount),
        bitangent * blurRadius,
        dominantDirection
    );
}

float2x2 GetRotationMatrix2x2(float angleInRadians)
{
    const float c = cos(angleInRadians);
    const float s = sin(angleInRadians);

    return float2x2(
        float2(c, -s),
        float2(s,  c)
    );
}

float3 GetPoissonDummyViewPosition(uint poissonSampleIndex, float3x3 samplingBasis, float2x2 rotationMatrix, float3 baseViewPosition)
{
    const float2 poissonSample = mul(g_PoissonSamples[poissonSampleIndex], rotationMatrix);

    // Real view sample position need to be reconstructed from depth
    // This one used to calculate 'sampleUv'
    const float3 dummyViewPosition = baseViewPosition + mul(float3(poissonSample, 0.0), samplingBasis); 
    return dummyViewPosition;
}

float2 GetPoissonSampleUv(float3 dummyViewPosition)
{
    const float4 clipPosition = mul(float4(dummyViewPosition, 1.0), g_FrameConstants.Camera.ViewToClip);
    const float3 ndcPosition = ClipPositionToNdcPosition(clipPosition);
    const float2 sampleUv = NdcPositionToUv(ndcPosition);

    return sampleUv;
}

float GetDistanceFromPointToPlane(float3 nonPlanePoint, float3 planePoint, float3 planeNormal)
{
    const float3 rayFromPlanePointToPoint = nonPlanePoint - planePoint;
    const float distance = dot(planeNormal, rayFromPlanePointToPoint); // Project the ray to plane normal

    return abs(distance);
}

float GetGeometryWeight(float3 baseViewPosition, float3 baseViewNormal, float3 sampleViewPosition, float accumulationSpeed)
{
    if (!g_PassConstants.IsGeometryWeightUsed)
    {
        return 1.0;
    }

    const float invMaxDistanceToPlane = g_PassConstants.GeometryWeightSensitivity / (1.0 + baseViewPosition.z);
    const float distanceToPlane = GetDistanceFromPointToPlane(sampleViewPosition, baseViewPosition, baseViewNormal);

    const float weight = saturate(1.0 - abs(distanceToPlane) * invMaxDistanceToPlane);
    return weight;
}

float GetNormalWeight(float3 baseViewNormal, float3 sampleViewNormal, float baseRoughness, float frameCount, float maxFrameCount)
{
    if (!g_PassConstants.IsNormalWeightUsed)
    {
        return 1.0;
    }

    // Close to zero if accumulation goes well
    // Make angle more narrow over time
    const float frameFactor = 1.0 - frameCount / maxFrameCount;
    
    float baseAngle = GetGgxSpecularLobeHalfAngleInRadians(baseRoughness);
    baseAngle *= frameFactor;
    baseAngle += DegreesToRadians(2.0); // Optional

    // #TODO: baseViewNormal and sampleViewNormal have 0.999 length which cause floating point error while calculating the cos
    const float cosOfSampleAngle = saturate(dot(baseViewNormal, sampleViewNormal));
    const float sampleAngle = acos(cosOfSampleAngle);

    if (sampleAngle > baseAngle)
    {
        return 0.0;
    }

    // 1.0 - when angle is 0
    // 0.0 - when angle approaches half GGX lobe angle
    return 1.0 - sampleAngle / baseAngle;
}

float GetRoughnessWeight(float baseRoughness, float sampleRoughness)
{
    if (!g_PassConstants.IsRoughnessWeightUsed)
    {
        return 1.0;
    }

    const float norm = baseRoughness * baseRoughness * 0.99 + 0.01;
    const float weight = abs(baseRoughness - sampleRoughness) * rcp(norm);

    return saturate(1.0 - weight);
}

[numthreads(joint::ThreadCount881_X, joint::ThreadCount881_Y, joint::ThreadCount881_Z)]
void CsMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Texture2D<float4> albedoAndRoughnessTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserBlurRc_AlbedoAndRoughnessTexture)];
    Texture2D<float4> worldNormalTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserBlurRc_WorldNormalTexture)];
    Texture2D<float> depthBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserBlurRc_DepthBuffer)];
    Texture2D<float4> velocityTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserBlurRc_VelocityTexture)];
    Texture2D<float> noisyVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserBlurRc_NoisyVisibilityBuffer)];
    Texture2D<float> reprojectedHistoryTexture = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserBlurRc_ReprojectedHistoryTexture)];

    RWTexture2D<float> temporalAccumulationBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserBlurRc_TemporalAccumulationBuffer)];
    RWTexture2D<float> denoisedVisibilityBuffer = ResourceDescriptorHeap[GetRootConstant(joint::DenoiserBlurRc_DenoisedVisibilityBuffer)];

    const float depth = depthBuffer[dispatchThreadId.xy];
    
    if (depth >= 1.0)
    {
        denoisedVisibilityBuffer[dispatchThreadId.xy] = 0.0;
        return;
    }

    if (!g_FrameConstants.IsDenoiserEnabled)
    {
        denoisedVisibilityBuffer[dispatchThreadId.xy] = noisyVisibilityBuffer[dispatchThreadId.xy];
        return;
    }

    const joint::CameraConstants cameraConstants = g_FrameConstants.Camera;

    const float roughness = albedoAndRoughnessTexture[dispatchThreadId.xy].w;

    const float3 motionVector = velocityTexture[dispatchThreadId.xy].xyz;
    const float3 worldNormal = worldNormalTexture[dispatchThreadId.xy].xyz;

    const float2 uv = DispatchThreadIdToUv(dispatchThreadId, g_FrameConstants.InvRenderResolution);
    const float2 previousUv = uv - motionVector.xy;
    const float previousDepth = depth - motionVector.z;
    const float3 previousViewPosition = ReconstructViewPositionFromDepth(previousUv, previousDepth, g_FrameConstants.PrevCamera.InvViewToClip);
    const float3 previousWorldPosition = ReconstructWorldPositionFromViewPosition(previousViewPosition, g_FrameConstants.PrevCamera.InvWorldToView);

    const float3 viewPosition = ReconstructViewPositionFromDepth(uv, depth, cameraConstants.InvViewToClip);
    const float3 worldPosition = ReconstructWorldPositionFromViewPosition(viewPosition, cameraConstants.InvWorldToView);

    const float3 lightDirection = normalize(cameraConstants.WorldPosition - worldPosition);
    const float nDotL = dot(worldNormal, lightDirection);
    const float parallax = CalcParallax(worldPosition, previousWorldPosition);
    const float allowedFrameCount = GetMaxAllowedAccumulatedFrameCountUsingSurfaceMotion(roughness, nDotL, parallax);

    float frameCount = temporalAccumulationBuffer[dispatchThreadId.xy];
    frameCount = min(frameCount, g_FrameConstants.MaxTemporalAccumulationCount);

    if (g_PassConstants.IsDenoiserAntilagEnabled)
    {
        frameCount = min(frameCount, allowedFrameCount);
    }

    const float normalizedFrameCount = frameCount / g_FrameConstants.MaxTemporalAccumulationCount;
    const float accumulationSpeed = 1.0 / (1.0 + frameCount);

    const float3 viewNormal = mul(worldNormal, (float3x3)cameraConstants.WorldToViewForNormals);

    const float blurRadius = lerp(g_PassConstants.MinBlurRadius, g_PassConstants.MaxBlurRadius, accumulationSpeed);
    const float3x3 samplingBasis = GetKernelBasis(viewPosition, viewNormal, roughness, blurRadius, normalizedFrameCount);
    const float2x2 rotationMatrix = GetRotationMatrix2x2(g_FrameConstants.ElapsedTimeInSec * 0.01);

    float sampleSum = 0.0;
    float sampleWeightSum = 0.0;

    sampleSum += noisyVisibilityBuffer[dispatchThreadId.xy];
    sampleWeightSum += 1.0;

    for (uint i = 0; i < g_PoissonSampleCount; ++i)
    {
        const float3 dummySampleViewPosition = GetPoissonDummyViewPosition(i, samplingBasis, rotationMatrix, viewPosition);
        const float2 sampleUv = GetPoissonSampleUv(dummySampleViewPosition);

        const float sampleDepth = depthBuffer.SampleLevel(g_PointWrapSampler, sampleUv, 0.0);
        const float sampleRoughness = albedoAndRoughnessTexture.SampleLevel(g_PointClampSampler, sampleUv, 0.0).w;
        const float3 sampleWorldNormal = worldNormalTexture.SampleLevel(g_PointClampSampler, sampleUv, 0.0).xyz;
        const float sample = noisyVisibilityBuffer.SampleLevel(g_PointClampSampler, sampleUv, 0.0);

        const float3 sampleViewPosition = ReconstructViewPositionFromDepth(sampleUv, sampleDepth, cameraConstants.InvViewToClip);

        const float geometryWeight = GetGeometryWeight(viewPosition, viewNormal, sampleViewPosition, accumulationSpeed);
        const float normalWeight = GetNormalWeight(worldNormal, sampleWorldNormal, roughness, frameCount, g_FrameConstants.MaxTemporalAccumulationCount);
        const float roughnessWeight = GetRoughnessWeight(roughness, sampleRoughness);
        const float sampleWeight = geometryWeight * normalWeight * roughnessWeight;

        sampleSum += sample * sampleWeight;
        sampleWeightSum += sampleWeight;
    }

    const float currentSample = sampleSum / sampleWeightSum;
    const float historySample = reprojectedHistoryTexture[dispatchThreadId.xy];
    const float denoisedSample = lerp(historySample, currentSample, accumulationSpeed);

    // temporalAccumulationBuffer[dispatchThreadId.xy] = frameCount;
    denoisedVisibilityBuffer[dispatchThreadId.xy] = !isnan(denoisedSample) ? denoisedSample : 0.0;
}
