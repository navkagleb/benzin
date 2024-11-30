#include "unified_root_parameters.hlsli"

#include "joint/sigma_denoiser_resources.hpp"
#include "sigma_denoiser/sigma_common.hlsli"

BenzinDeclareRootResource(Texture2D<float>, g_ViewDepthTex, joint::Rc_SigmaClassifyTiles::ViewDepthTex);
BenzinDeclareRootResource(Texture2D<float>, g_PenumbraTex, joint::Rc_SigmaClassifyTiles::PenumbraTex);
BenzinDeclareRootResource(RWTexture2D<float4>, g_OutTilesTex, joint::Rc_SigmaClassifyTiles::OutTilesTex);

groupshared uint gs_TileMask;
groupshared uint gs_TileRadius; // Stores float value. Use asuint and asfloat

struct CsInput
{
    uint2 ThreadPos : SV_GroupThreadID;
    uint2 GroupPos : SV_GroupID;
    uint FlatThreadIndex : SV_GroupIndex;
};

static const uint g_ThreadCountX = 8;
static const uint g_ThreadCountY = 4;
static const uint g_ThreadCountZ = 1;

static const uint2 g_ThreadTileSize = joint::g_SigmaTileSize / uint2(g_ThreadCountX, g_ThreadCountY);

void FetchThreadTileInfo(CsInput input, out uint outThreadMask, out float outThreadRadius)
{
    const uint2 basePixelPos = input.GroupPos * joint::g_SigmaTileSize + input.ThreadPos * g_ThreadTileSize;

    uint threadMask = 0;
    float threadRadius = 0.0;

    [unroll]
    for (uint i = 0; i < g_ThreadTileSize.x; ++i)
    {
        [unroll]
        for (uint j = 0; j < g_ThreadTileSize.y; ++j)
        {
            const uint2 pixelPos = basePixelPos + uint2(i, j);

            const float penumbra = g_PenumbraTex[pixelPos];
            const float viewDepth = g_ViewDepthTex[pixelPos];

            const bool isInf = viewDepth > sigma::g_DenoisingRange;
            const bool isShadow = penumbra == 0;
            const bool isLit = sigma::IsLit(penumbra);

            threadMask += ((isLit || isInf || isShadow) ? 1 : 0) << 0;
            threadMask += ((!isLit || isInf || isShadow) ? 1 : 0) << 9;
            threadMask += (isInf ? 1 : 0) << 18;

            const float hitDistance = isLit || isInf ? 0.0 : penumbra;
            const float unprojectDepth = sigma::PixelRadiusToWorld(1.0, g_FrameConstants.PixelToWorldScale, viewDepth);
            const float pixelRadius = sigma::GetKernelPixelRadius(hitDistance, unprojectDepth);

            threadRadius = max(pixelRadius, threadRadius);
        }
    }
     
    outThreadMask = threadMask;
    outThreadRadius = threadRadius;
}

[numthreads(g_ThreadCountX, g_ThreadCountY, g_ThreadCountZ)]
void CsMain(CsInput input)
{
    // Cpp. Thread group size = 16 => sample count per thread group = 16 * 16 = 256
    // Hlsl. Sample count per thread group = thread count * sample count per thread = (8 * 4) * (2 * 4) = 256

    if (input.FlatThreadIndex == 0)
    {
        gs_TileMask = 0;
        gs_TileRadius = 0;
    }

    GroupMemoryBarrier();
    {
        uint threadMask = 0;
        float threadRadius = 0.0;
        FetchThreadTileInfo(input, threadMask, threadRadius);

        InterlockedAdd(gs_TileMask, threadMask);
        InterlockedMax(gs_TileRadius, asuint(threadRadius));
    }
    GroupMemoryBarrier();

    if (input.FlatThreadIndex == 0)
    {
        // groupThreadCount = 8 * 4 = 32
        // samplePerThreadCount = 4 * 2 = 8
        // maxThreadMaskValue = 32 * 8 = 256 = 2 ^ 8 => need 8 bits to store max value
        // threadMask = 2 ^ 9 - 1 = 512 - 1 = 511 => all 8 bits are equal to 1

        // umbra - fully shadowed
        // penumbra - partially lit

        const bool isLit = ((gs_TileMask >> 0) & 511) == 256;
        const bool isUmbra = ((gs_TileMask >> 9) & 511) == 256;
        const bool isInf = ((gs_TileMask >> 18) & 511) == 256;

        float4 result;
        result.x = (isLit || isUmbra) ? 0.0 : 1.0; // Mark penumbra regions
        result.y = saturate(asfloat(gs_TileRadius) / (float)joint::g_SigmaTileSize);
        result.z = isInf ? 1.0 : 0.0;
        result.w = 0.0;

        g_OutTilesTex[input.GroupPos] = result;
    }
}
