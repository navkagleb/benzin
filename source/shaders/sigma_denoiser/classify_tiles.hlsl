#include "unified_root_parameters.hlsli"

#include "joint/sigma_denoiser_resources.hpp"
#include "sigma_denoiser/sigma_common.hlsli"

BenzinDeclareRootResource(Texture2D<float>, g_ViewDepthTex, joint::SigmaClassifyTilesRc_ViewDepthTex);
BenzinDeclareRootResource(Texture2D<float>, g_PenumbraTex, joint::SigmaClassifyTilesRc_PenumbraTex);
BenzinDeclareRootResource(RWTexture2D<float4>, g_OutTilesTex, joint::SigmaClassifyTilesRc_OutTilesTex);

uint GetBitCount(uint value)
{
    return 32 - firstbithigh(value);
}

static const uint g_TileSize = 16;

groupshared uint g_TileMask;
groupshared uint g_TileRadius;

struct CsInput
{
    uint2 ThreadPos : SV_GroupThreadID;
    uint2 GroupPos : SV_GroupID;
    uint FlatThreadIndex : SV_GroupIndex;
};

[numthreads(8, 4, 1)]
void CsMain(CsInput input)
{
    if (input.FlatThreadIndex == 0)
    {
        g_TileMask = 0;
        g_TileRadius = 0;
    }

    GroupMemoryBarrier();

    const uint2 basePixelPos = input.GroupPos * g_TileSize + input.ThreadPos * uint2( 2 /* 16 / 8 */, 4 /* 16 / 4 */);

    uint threadMask = 0;
    float threadRadius = 0.0;

    [unroll]
    for (uint i = 0; i < 2; ++i)
    {
        [unroll]
        for (uint j = 0; j < 4; ++j)
        {
            const uint2 pixelPos = basePixelPos + uint2(i, j);
            const float2 uv = (pixelPos + 0.5) * g_FrameConstants.InvRenderResolution;

            // TODO: Load through operator []
            const float penumbra = g_PenumbraTex.SampleLevel(g_PointClampSampler, uv, 0.0);
            const float viewDepth = g_ViewDepthTex.SampleLevel(g_PointClampSampler, uv, 0.0);

            const bool isInf = viewDepth > sigma::g_DenoisingRange;
            const bool isShadow = penumbra == 0; // TODO: NRD sample has reverted shadow !!!
            const bool isLit = sigma::IsLit(penumbra);

            threadMask += ((isLit || isInf || isShadow) ? 1 : 0) << 0;
            threadMask += ((!isLit || isInf || isShadow) ? 1 : 0) << 9;
            threadMask += (isInf ? 1 : 0) << 18;

            const float hitDistance = isLit || isInf ? 0.0 : penumbra;
            const float unprojectDepth = sigma::PixelRadiusToWorldAtDepth(1.0, viewDepth);
            const float pixelRadius = sigma::GetKernelRadiusInPixels(hitDistance, unprojectDepth);

            threadRadius = max(pixelRadius, threadRadius);
        }
    }

    InterlockedAdd(g_TileMask, threadMask);
    InterlockedMax(g_TileRadius, asuint(threadRadius));

    GroupMemoryBarrier();

    if (input.FlatThreadIndex == 0)
    {
        // groupThreadCount = 8 * 4 = 32
        // samplePerThreadCount = 4 * 2 = 8
        // maxThreadMaskValue = 32 * 8 = 256 = 2 ^ 8 => need 8 bits to store max value
        // threadMask = 2 ^ 9 - 1 = 512 - 1 = 511 => all 8 bits are equal to 1

        // umbra - fully shadowed
        // penumbra - partially lit

        const bool isLit = ((g_TileMask >> 0) & 511) == 256;
        const bool isUmbra = ((g_TileMask >> 9) & 511) == 256;
        const bool isInf = ((g_TileMask >> 18) & 511) == 256;

        float4 result;
        result.x = (isLit || isUmbra) ? 0.0 : 1.0; // Mark penumbra regions
        result.y = saturate(asfloat(g_TileRadius) / asfloat(g_TileSize)); // TODO: what is 16.0 ??? TileSize ???
        result.z = isInf ? 1.0 : 0.0;
        result.w = 0.0;

        g_OutTilesTex[input.GroupPos] = result;
    }
}
