#define g_ThreadCountX 8
#define g_ThreadCountY 16

#define SIGMA_USE_BORDER_2

#include "joint/sigma_denoiser_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "bilinear_filter.hlsli"
#include "sigma_denoiser/group_shared_preloader.hlsli"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_Mv, joint::SigmaTemporalStabilizationResources::Mv);
BenzinDeclareRootResource(Texture2D<float>, g_ViewDepth, joint::SigmaTemporalStabilizationResources::ViewDepth);
BenzinDeclareRootResource(Texture2D<float2>, g_SmoothTiles, joint::SigmaTemporalStabilizationResources::SmoothTiles);
BenzinDeclareRootResource(Texture2D<float>, g_Penumbra, joint::SigmaTemporalStabilizationResources::Penumbra);
BenzinDeclareRootResource(Texture2D<float>, g_Shadow, joint::SigmaTemporalStabilizationResources::Shadow);
BenzinDeclareRootResource(Texture2D<float>, g_ShadowHistory, joint::SigmaTemporalStabilizationResources::ShadowHistory);
BenzinDeclareRootResource(Texture2D<uint>, g_HistoryLength, joint::SigmaTemporalStabilizationResources::HistoryLength);

BenzinDeclareRootResource(RWTexture2D<float>, g_OutShadow, joint::SigmaTemporalStabilizationResources::OutShadow);
BenzinDeclareRootResource(RWTexture2D<uint>, g_OutHistoryLength, joint::SigmaTemporalStabilizationResources::OutHistoryLength);

struct PixelData
{
    float Penumbra;
    float ViewDepth;
    float Shadow;
};

groupshared PixelData g_Pixels[g_SharedBufferSizeY][g_SharedBufferSizeX];

void Preload(uint2 sharedPos, uint2 pixelPos)
{
    PixelData pixel;
    pixel.Penumbra = g_Penumbra[pixelPos];
    pixel.ViewDepth = g_ViewDepth[pixelPos];
    pixel.Shadow = sigma::UnpackShadow(g_Shadow[pixelPos]);

    g_Pixels[sharedPos.y][sharedPos.x] = pixel;
}

uint PackViewDepthAndHistoryLength(float viewDepth, float historyLength)
{
    uint packed = asuint(viewDepth) & ~SIGMA_TS_MAX_HISTORY_LENGTH;
    packed |= min(uint(historyLength + 0.5), SIGMA_TS_MAX_HISTORY_LENGTH);

    return packed;
}

void UnpackViewDepthAndHistoryLength(uint4 packedData, out float4 outViewDepths, out float4 outHistoryLengths)
{
    outViewDepths = asfloat(packedData & ~SIGMA_TS_MAX_HISTORY_LENGTH);
    outHistoryLengths = float4(packedData & SIGMA_TS_MAX_HISTORY_LENGTH);
}

void CalcLocalVariance(uint2 threadPos, float centerPenumbra, out float outM1, out float outM2)
{
    outM1 = 0.0;
    outM2 = 0.0;

    float weightSum = 0.0;

    [unroll]
    for (int j = -SIGMA_BORDER; j <= SIGMA_BORDER; ++j)
    {
        [unroll]
        for (int i = -SIGMA_BORDER; i <= SIGMA_BORDER; ++i)
        {
            const int2 sharedPos = threadPos + SIGMA_BORDER + int2(i, j);
            const PixelData pixel = g_Pixels[sharedPos.y][sharedPos.x];

            float shadowWeight = 1.0;

            const bool isCenterSample = i == 0 && j == 0;
            if (!isCenterSample)
            {
                shadowWeight = sigma::IsBothLitOrUmbra(centerPenumbra, pixel.Penumbra);
                shadowWeight *= sigma::GetGaussianWeight(length(float2(i, j) / SIGMA_BORDER));
            }

            outM1 += pixel.Shadow * shadowWeight;
            outM2 += pixel.Shadow * pixel.Shadow * shadowWeight;
            weightSum += shadowWeight;
        }
    }

    const float invWeightSum = 1.0 / weightSum; // rcp(shadowWeightSum)?
    outM1 *= invWeightSum;
    outM2 *= invWeightSum;
}

float GetStdDeviation(float m1, float m2)
{
    const float variance = abs(m2 - m1 * m1);
    const float sigma = sqrt(variance);

    return sigma;
}

void CalcPrevPositions(uint2 pixelPos, float2 pixelUv, float viewDepth, out float2 outPrevPixelUv, out float3 outPrevViewPos)
{
    const float3 viewPos = ReconstructViewPosition(pixelUv, viewDepth, GetCameraConsts().UvToViewScale, GetCameraConsts().UvToViewBias);
    const float3 worldPos = mul(float4(viewPos, 1.0), GetCameraConsts().ViewToWorld).xyz;

    float3 mv = g_Mv[pixelPos].xyz;
    mv.xy *= g_FrameConsts.InvRenderResolution; // TODO: Pack/Unpack Mv

    const float2 prevPixelUv = pixelUv - mv.xy;

    const float prevViewDepth = viewDepth - mv.z;
    const float3 tempPrevViewPos = ReconstructViewPosition(prevPixelUv, prevViewDepth, GetPrevCameraConsts().UvToViewScale, GetPrevCameraConsts().UvToViewBias); // TODO: Does there is any difference between 'prevViewPos'?
    const float3 prevWorldPos = mul(float4(tempPrevViewPos, 1.0), GetPrevCameraConsts().ViewToWorld).xyz;
    const float3 prevViewPos = mul(float4(prevWorldPos, 1.0), GetPrevCameraConsts().WorldToView).xyz;

    outPrevPixelUv = prevPixelUv;
    outPrevViewPos = prevViewPos;
}

float GetDisocclusionThreshold(float viewDepth)
{
    // Only for viewDepth comparisons for close to each other pixels (not sparse filters!)

    const float worldFrustumSize = sigma::PixelsToWorldSize(g_FrameConsts.MinRenderDimension, GetCameraConsts().PixelToWorldScale, viewDepth);

    return worldFrustumSize * g_PassConsts0.DisocclusionThreshold;
}

void SampleHistoryData(float2 prevPixelUv, float viewDepth, float prevViewDepth, out float outHistoryLength, out float outShadowHistory)
{
    // History length
    const BilinearFilter prevFilter = CreateBilinearFilter(prevPixelUv, g_FrameConsts.RenderResolution);

    const float2 gatherUv = (prevFilter.TopLeftTexelPos + 1.0) * g_FrameConsts.InvRenderResolution;
    const uint4 prevHistoryData = g_HistoryLength.GatherRed(g_PointClampSampler, gatherUv).wzxy;

    float4 prevViewDepths;
    float4 prevHistoryLengths;
    UnpackViewDepthAndHistoryLength(prevHistoryData, prevViewDepths, prevHistoryLengths);

    float disocclusionThreshold = GetDisocclusionThreshold(viewDepth); // TODO: slope scale?
    disocclusionThreshold *= sigma::IsUvIn01Range(prevPixelUv);
    disocclusionThreshold -= sigma::g_Eps;

    const float4 prevPlaneDistanceDiff = abs(prevViewDepths - prevViewDepth);
    const float4 isSampleOccluded = step(prevPlaneDistanceDiff, disocclusionThreshold);
    const float4 customWeights = ExpandBilinearWeights(prevFilter.LerpWeights) * isSampleOccluded;

    const float historyLength = ApplyBilinearCustomWeights(prevHistoryLengths, customWeights);

    // Shadow history
    const bool isCatRomAllowed = dot(customWeights, 1.0) > 3.5;

    // const bool isBicubicSamplingUsed = dot(customWeights, 1.0) > 3.5;
    // const float history = SampleShadowHistory(prevPixelUv, isBicubicSamplingUsed);

    float shadowHistory;
    BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights(
        saturate(prevPixelUv) * g_FrameConsts.RenderResolution,
        g_FrameConsts.InvRenderResolution,
        customWeights,
        isCatRomAllowed,
        g_ShadowHistory,
        shadowHistory
    );

    shadowHistory = g_ShadowHistory.SampleLevel(g_LinearClampSampler, prevPixelUv, 0.0); // TODO: Remove?
    shadowHistory = saturate(shadowHistory);
    shadowHistory = sigma::UnpackShadow(shadowHistory);

    outHistoryLength = historyLength;
    outShadowHistory = shadowHistory;
}

float CalcAntilagFactor(float history, float clampedHistory)
{
    float antilag = abs(clampedHistory - history);
    antilag = sqrt(saturate(antilag));
    antilag = saturate(1.0 - antilag);

    return antilag;
}

#if 0
float SampleShadowHistory(float2 prevPixelUv, bool isBicubicSamplingUsed)
{
    float history = isBicubicSamplingUsed && g_PassConsts0.IsBicubicSamplingUsedForHistory
        ? BicubicFilterNoCorners(g_ShadowHistory, saturate(prevPixelUv) * g_FrameConsts.RenderResolution, g_FrameConsts.InvRenderResolution).x
        : g_ShadowHistory.SampleLevel(g_LinearClampSampler, prevPixelUv, 0.0).x;

    history = saturate(history);
    history = sigma::UnpackShadow(history);

    return history;
}
#endif

[numthreads(g_ThreadCountX, g_ThreadCountY, 1)]
void CsMain(sigma::GroupSharedCsInput input)
{
    bool isSky = SIGMA_USE_TILE_CHECK;
    isSky = isSky && g_SmoothTiles[input.PixelPos >> 4].y;

    if (!isSky)
    {
        // TODO: Will it still work even if it is false?
        SigmaPreloadToGroupSharedMem(input, g_FrameConsts.RenderResolution, Preload);
    }

    GroupMemoryBarrierWithGroupSync();

    const uint2 sharedPos = input.ThreadPos + SIGMA_BORDER;
    const PixelData centerPixel = g_Pixels[sharedPos.y][sharedPos.x];

    const bool isOutOfBounds = any(input.PixelPos >= g_FrameConsts.RenderResolution);
    const bool isOutOfDenoisingRange = centerPixel.ViewDepth > SIGMA_DENOISING_RANGE;
    if (isSky || isOutOfBounds || isOutOfDenoisingRange)
    {
        return;
    }

    const float2 pixelUv = (input.PixelPos + 0.5) * g_FrameConsts.InvRenderResolution;
    const float tileValue = sigma::TextureCubicX(g_SmoothTiles, pixelUv);

    bool isHardShadow = SIGMA_TS_USE_EARLY_OUT;
    isHardShadow &= (SIGMA_USE_TILE_CHECK && tileValue == 0.0) || centerPixel.Penumbra == 0.0;
    if (isHardShadow)
    {
        g_OutShadow[input.PixelPos] = sigma::PackShadow(centerPixel.Shadow);
        g_OutHistoryLength[input.PixelPos] = PackViewDepthAndHistoryLength(centerPixel.ViewDepth, SIGMA_TS_MAX_HISTORY_LENGTH); // TODO: yes, SIGMA_MAX_HISTORY_LENGTH to allow accumulation in neighbors

        return;
    }

    float m1;
    float m2;
    CalcLocalVariance(input.ThreadPos, centerPixel.Penumbra, m1, m2);

    float2 prevPixelUv;
    float3 prevViewPos;
    CalcPrevPositions(input.PixelPos, pixelUv, centerPixel.ViewDepth, prevPixelUv, prevViewPos);

    float historyLength;
    float history;
    SampleHistoryData(prevPixelUv, centerPixel.ViewDepth, prevViewPos.z, historyLength, history);

    float sigma = GetStdDeviation(m1, m2);
    sigma *= lerp(SIGMA_TS_SIGMA_SCALE, 1.0, 1.0 / (1.0 + historyLength)); // TODO: lerp(SIGMA_TS_SIGMA_SCALE, 1.0, 0.125) != SIGMA_TS_SIGMA_SCALE

    float clampedHistory = clamp(history, m1 - sigma, m1 + sigma);

    historyLength *= CalcAntilagFactor(history, clampedHistory);

    const float historyWeight = historyLength / (1.0 + historyLength); // The greater the accumulation, the greater the influence of history

    // Street magic (helps to smooth out "penumbra to 1" regions)
    float streetMagic = 0.6 * historyWeight;
    clampedHistory = lerp(clampedHistory, history, streetMagic);

    const float shadowResult = lerp(centerPixel.Shadow, clampedHistory, min(g_PassConsts0.StabilizationStrength, historyWeight));
    const float historyLengthResult = min(historyLength + 1.0, SIGMA_TS_MAX_HISTORY_LENGTH);

    g_OutShadow[input.PixelPos] = sigma::PackShadow(shadowResult);
    g_OutHistoryLength[input.PixelPos] = PackViewDepthAndHistoryLength(centerPixel.ViewDepth, historyLengthResult);
}
