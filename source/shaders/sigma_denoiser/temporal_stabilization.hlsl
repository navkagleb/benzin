// #define DEBUG_TEMPORAL_STABILIZATION
#define SIGMA_USE_BORDER_2

#include "joint/sigma_denoiser_resources.hpp"

#define RenderPassConstantsType joint::SigmaConstants
#include "unified_root_parameters.hlsli"

#include "filters.hlsli"
#include "gbuffer.hlsli"
#include "sigma_denoiser/lds_preloader.hlsli"
#include "sigma_denoiser/sigma_common.hlsli"
#include "space_convertions.hlsli"

BenzinDeclareRootResource(Texture2D<float>, g_ViewDepthTex, joint::Rc_SigmaTemporalStabilization::ViewDepthTex);
BenzinDeclareRootResource(Texture2D<float4>, g_MvTex, joint::Rc_SigmaTemporalStabilization::MvTex);
BenzinDeclareRootResource(Texture2D<float>, g_PenumbraTex, joint::Rc_SigmaTemporalStabilization::PenumbraTex);
BenzinDeclareRootResource(Texture2D<float>, g_ShadowTex, joint::Rc_SigmaTemporalStabilization::ShadowTex);
BenzinDeclareRootResource(Texture2D<float4>, g_HistoryTex, joint::Rc_SigmaTemporalStabilization::HistoryTex);
BenzinDeclareRootResource(Texture2D<float2>, g_SmoothTilesTex, joint::Rc_SigmaTemporalStabilization::SmoothTilesTex);

BenzinDeclareRootResource(RWTexture2D<float>, g_OutShadowTex, joint::Rc_SigmaTemporalStabilization::OutShadowTex);

static const uint g_ThreadCountX = 8;
static const uint g_ThreadCountY = 16;

static const uint g_SharedBufferSizeX = SigmaCalcSharedBufferSize(g_ThreadCountX);
static const uint g_SharedBufferSizeY = SigmaCalcSharedBufferSize(g_ThreadCountY);

struct SharedData
{
    float Penumbra;
    float ViewDepth;
    float Shadow;
    float SignNoL;
};

groupshared SharedData g_SharedData[g_SharedBufferSizeY][g_SharedBufferSizeX];

float IsInScreenNearest(float2 uv)
{
    return float(all(uv >= 0.0) && all(uv < 1.0));
}

void Preload(uint2 sharedPos, uint2 globalPos)
{
    SharedData data;
    data.Penumbra = g_PenumbraTex[globalPos];
    data.ViewDepth = g_ViewDepthTex[globalPos];
    data.Shadow = sigma::UnpackShadow(g_ShadowTex[globalPos]);
    data.SignNoL = float(data.Penumbra != 0.0);

    g_SharedData[sharedPos.y][sharedPos.x] = data;
}

#define SIGMA_SHOW 0 // 1 - tiles, 2 - history weight

#define SIGMA_TS_EARLY_OUT_THRESHOLD 0.25
#define SIGMA_TS_Z_FALLOFF 1.0 // exp2( -SIGMA_TS_Z_FALLOFF * dz )

struct CsInput
{
    uint2 ThreadPos : SV_GroupThreadID;
    uint2 PixelPos : SV_DispatchThreadID;
    uint FlatThreadIndex : SV_GroupIndex;
};

void CalcLocalVariance(
    CsInput input,
    SharedData centerData,
    out float outM1,
    out float outM2,
    out float outNearestViewDepth,
    out uint2 outNearestViewDepthPixelOffset
)
{
    float shadowWeightSum = 0.0;
    float m1 = 0.0;
    float m2 = 0.0;

    float nearestViewDepth = sigma::g_DenoisingRange;
    uint2 nearestViewDepthPixelOffset = 0;

    [unroll]
    for (uint j = 0; j <= SIGMA_BORDER * 2; ++j)
    {
        [unroll]
        for (uint i = 0; i <= SIGMA_BORDER * 2; ++i)
        {
            const uint2 pos = input.ThreadPos + int2(i, j);
            const SharedData data = g_SharedData[pos.y][pos.x];

            float shadowWeight = 1.0;
            if (!(i == SIGMA_BORDER && j == SIGMA_BORDER))
            {
                shadowWeight *= exp2(-SIGMA_TS_Z_FALLOFF * abs(data.ViewDepth - centerData.ViewDepth)); // soft Z test // TODO: use relative difference?
                shadowWeight *= sigma::IsLit(data.Penumbra) == sigma::IsLit(centerData.Penumbra); // no-harm on a flat surface due to wide spatials, needed to prevent bleeding from one surface to another
                shadowWeight *= float(centerData.ViewDepth < sigma::g_DenoisingRange); // ignore sky
                shadowWeight *= float(centerData.SignNoL == data.SignNoL); // ignore samples with different NoL signs
            }

            if (nearestViewDepth > data.ViewDepth)
            {
                nearestViewDepth = data.ViewDepth;
                nearestViewDepthPixelOffset = uint2(i, j);
            }

            m1 += data.Shadow * shadowWeight;
            m2 += data.Shadow * data.Shadow * shadowWeight;
            shadowWeightSum += shadowWeight;
        }
    }

    const float invShadowWeightSum = 1.0 / shadowWeightSum; // rcp(shadowWeightSum);
    m1 *= invShadowWeightSum;
    m2 *= invShadowWeightSum;

    outM1 = m1;
    outM2 = m2;
    outNearestViewDepth = nearestViewDepth;
    outNearestViewDepthPixelOffset = nearestViewDepthPixelOffset;
}

float GetStdDeviation(float m1, float m2)
{
    // sigma = standard deviation, variance = sigma ^ 2

    return sqrt(abs(m2 - m1 * m1)); // sqrt( max( m2 - m1 * m1, 0.0 ) )
}

float2 GetPrevPixelUv(uint2 pixelPosition, uint2 nearestViewDepthPixelOffset)
{
    const uint2 mvPixelPosition = clamp(pixelPosition + nearestViewDepthPixelOffset - SIGMA_BORDER, 0, g_FrameConstants.RenderResolution);
    const float2 mv = g_MvTex[mvPixelPosition].xy;

    const float2 pixelUv = (pixelPosition + 0.5) * g_FrameConstants.InvRenderResolution;
    const float2 prevPixelUv = pixelUv - mv.xy;

    return prevPixelUv;
}

float SampleHistory(float2 prevPixelUv)
{
    float history = g_PassConstants.IsBicubicSamplingUsedForHistory
        ? BicubicFilterNoCorners(g_HistoryTex, saturate(prevPixelUv) * g_FrameConstants.RenderResolution, g_FrameConstants.InvRenderResolution).x
        : g_HistoryTex.SampleLevel(g_LinearClampSampler, prevPixelUv, 0.0).x;

    history = saturate(history);
    history = sigma::UnpackShadow(history);

    return history;
}

float ClampHistory(float history, float thisFrameShadow, float sigma)
{
    static const float sigmaScale = 3.0;

    const float inputMin = thisFrameShadow - sigma * sigmaScale;
    const float inputMax = thisFrameShadow + sigma * sigmaScale;

    const float clampedHistory = clamp(history, inputMin, inputMax);
    return clampedHistory;
}

float CalcAntilagFactor(float fast, float slow, float sigma)
{
    static const float antilagSigmaScale = 0.25;
    static const float antilagEps = 0.05;
    static const float antilagPower = 1.0;

    const float a = abs(slow - fast) - sigma * antilagSigmaScale - antilagEps;
    const float b = max(slow, fast) + sigma * antilagSigmaScale + antilagEps;

    float antilag = a / b;
    antilag = smoothstep(0.0, 1.0, saturate(1.0 - antilag));
    antilag = pow(saturate(antilag), antilagPower);

    return antilag;
}

float CalcHistoryWeight(float2 prevPixelUv, float antilagFactor, float penumbraInPixels)
{
    static const float maxHistoryWeight = 0.95;
    static const float earlyOutThreshold = 0.25;

    float historyWeight = maxHistoryWeight;
    historyWeight *= IsInScreenNearest(prevPixelUv);
    historyWeight *= antilagFactor;
    historyWeight *= smoothstep(earlyOutThreshold, 1.0, penumbraInPixels);
    historyWeight *= g_PassConstants.StabilizationStrength;

    return historyWeight;
}

[numthreads(g_ThreadCountX, g_ThreadCountY, 1)]
void CsMain(CsInput input)
{
    const bool isSky = g_SmoothTilesTex[input.PixelPos >> 4].y;

    if (!isSky)
    {
        sigma::LdsPreloadCreation creation;
        creation.ThreadPos = input.ThreadPos;
        creation.PixelPos = input.PixelPos;
        creation.FlatThreadIndex = input.FlatThreadIndex;
        creation.GroupSize = uint2(g_ThreadCountX, g_ThreadCountY);
        creation.BufferSize = uint2(g_SharedBufferSizeX, g_SharedBufferSizeY);
        creation.Dimension = g_FrameConstants.RenderResolution;

        SigmaRunLdsPreloader(creation, Preload);

        GroupMemoryBarrierWithGroupSync();
    }
    
    // Tile-based early out
    if (isSky || any(input.PixelPos >= g_FrameConstants.RenderResolution))
    {
        return;
    }
    
    // Center data
    const uint2 sharedPos = input.ThreadPos + SIGMA_BORDER;
    const SharedData centerData = g_SharedData[sharedPos.y][sharedPos.x];

    // Early out
    if (centerData.ViewDepth > sigma::g_DenoisingRange)
    {
        return;
    }

    // Early out
    const float unprojectDepth = sigma::PixelRadiusToWorldAtDepth(g_FrameConstants.PixelToWorldScale, 1.0, centerData.ViewDepth);
    const float penumbraInPixels = centerData.Penumbra / unprojectDepth;

#if !defined(DEBUG_TEMPORAL_STABILIZATION)
    if (penumbraInPixels <= SIGMA_TS_EARLY_OUT_THRESHOLD && SIGMA_SHOW == 0)
    {
        g_OutShadowTex[input.PixelPos] = sigma::PackShadow(centerData.Shadow);
        return;
    }
#endif

    // Local variance
    float m1 = 0.0;
    float m2 = 0.0;
    float nearestViewDepth = 0.0;
    uint2 nearestViewDepthPixelOffset = 0;
    CalcLocalVariance(input, centerData, m1, m2, nearestViewDepth, nearestViewDepthPixelOffset);

    const float sigma = GetStdDeviation(m1, m2);

    const float2 prevPixelUv = GetPrevPixelUv(input.PixelPos, nearestViewDepthPixelOffset);
    const float history = SampleHistory(prevPixelUv);
    const float antilag = CalcAntilagFactor(m1, history, sigma);
    
    const float historyWeight = CalcHistoryWeight(prevPixelUv, antilag, penumbraInPixels);
    const float clampedHistory = ClampHistory(history, m1, sigma);

    float result = lerp(centerData.Shadow, clampedHistory, historyWeight);

#if defined(DEBUG_TEMPORAL_STABILIZATION)
    float tileValue = g_SmoothTilesTex[input.PixelPos >> 4].x;
    tileValue = float(tileValue != 0.0); // optional, just to show fully discarded tiles

    result = tileValue;
    result = sigma::UnpackShadow(historyWeight);

    // Show grid
    result *= all((input.PixelPos & 15) != 0);
#endif

    g_OutShadowTex[input.PixelPos] = sigma::PackShadow(result);
}