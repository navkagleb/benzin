#include "joint/tone_mapping_resources.hpp"
#include "unified_root_parameters.hlsli"

#include "color_convertions.hlsli"
#include "luminance_histogram.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_HdrColor, joint::CalcLuminanceHistogramResources::HdrColor);
BenzinDeclareRootResource(RWBuffer<uint>, g_OutLuminanceHistogram, joint::CalcLuminanceHistogramResources::OutLuminanceHistogram);
BenzinDeclareRootResource(RWTexture2D<float>, g_OutDebugLuminanceHistogram, joint::CalcLuminanceHistogramResources::OutDebugLuminanceHistogram);

#define g_ThreadCountX 16
#define g_ThreadCountY 16

groupshared uint g_GroupLuminanceHistogram[g_ThreadCountX * g_ThreadCountY];

[numthreads(g_ThreadCountX, g_ThreadCountY, 1)]
void CsMain(uint2 pixelPosition : SV_DispatchThreadID, uint binIndex : SV_GroupIndex)
{
    g_GroupLuminanceHistogram[binIndex] = 0;
    GroupMemoryBarrierWithGroupSync();

    if (all(pixelPosition < g_FrameConsts.RenderResolution))
    {
        const float luminance = RgbToLuminance(g_HdrColor[pixelPosition].xyz);
        const uint groupBinIndex = LuminanceToBinIndex(luminance);
        InterlockedAdd(g_GroupLuminanceHistogram[groupBinIndex], 1);
    }

    // Wait for all threads in the work group to reach this point before adding our
    // local histogram to the global one
    GroupMemoryBarrierWithGroupSync();
    
    // Technically there's no chance that two threads write to the same bin here,
    // but different work groups might! So we still need the atomic add
    InterlockedAdd(g_OutLuminanceHistogram[binIndex], g_GroupLuminanceHistogram[binIndex]);

    {
        // TODO
        // To visualize histogram buffer distribution
        const float maxBinValue = (g_FrameConsts.RenderResolution.x * g_FrameConsts.RenderResolution.y) / (g_ThreadCountX * g_ThreadCountY);
        g_OutDebugLuminanceHistogram[pixelPosition] = (float)g_OutLuminanceHistogram[binIndex] / maxBinValue;
    }
}
