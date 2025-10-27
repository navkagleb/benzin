// Ref: https://www.alextardif.com/HistogramLuminance.html - Adaptive Exposure from Luminance Histograms

#include "joint/tone_mapping_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "luminance_histogram.hlsli"

float AdaptLuminance(float luminance, float prevLuminance)
{
    return prevLuminance + (luminance - prevLuminance) * (1.0 - exp(-g_FrameConsts.DeltaTimeInSec * g_PassConsts.LuminanceHistogram.TimeFactor));
}

BenzinDeclareRootResource(RWBuffer<uint>, g_OutLuminanceHistogram, joint::CalcAvgLuminanceResources::OutLuminanceHistogram);
BenzinDeclareRootResource(RWTexture2D<float>, g_OutAvgLuminance, joint::CalcAvgLuminanceResources::OutAvgLuminance);

#define g_ThreadCountX 16
#define g_ThreadCountY 16

groupshared uint g_LuminanceHistogram[g_ThreadCountX * g_ThreadCountY];

[numthreads(g_ThreadCountX, g_ThreadCountY, 1)]
void CsMain(uint2 pixelPosition : SV_DispatchThreadID, uint binIndex : SV_GroupIndex)
{
    // Get the count from the histogram buffer
    const uint binPixelCount = g_OutLuminanceHistogram[binIndex];
    g_LuminanceHistogram[binIndex] = binPixelCount * binIndex; // TODO: Why we multiply by groupThreadIndex. (Weight sum)

    GroupMemoryBarrierWithGroupSync();

    g_OutLuminanceHistogram[binIndex] = 0;

    // This loop will perform a weighted count of the luminance range
    [unroll]
    for (uint cutoff = (g_ThreadCountX * g_ThreadCountY) >> 1; cutoff > 0; cutoff >>= 1)
    {
        if (binIndex < cutoff)
        {
            g_LuminanceHistogram[binIndex] += g_LuminanceHistogram[binIndex + cutoff];
        }

        GroupMemoryBarrierWithGroupSync();
    }

    if (binIndex == 0)
    {
        // Here we take our weighted sum and divide it by the number of pixels
        // that had luminance greater than zero (since the binIndex == 0, we can
        // use countForThisBin to find the number of black pixels)
        const uint pixelCount = g_FrameConsts.RenderResolution.x * g_FrameConsts.RenderResolution.y;
        const uint nonBlackPixelCount = max(pixelCount - binPixelCount, 1.0);

        const float medianBinIndex = g_LuminanceHistogram[0] / nonBlackPixelCount;
        const float avgLuminance = BinIndexToLuminance(medianBinIndex);
        const float prevLuminance = g_OutAvgLuminance[uint2(0, 0)];

        const float adaptedLuminance = AdaptLuminance(avgLuminance, prevLuminance);
        g_OutAvgLuminance[uint2(0, 0)] = adaptedLuminance;
    }
}
