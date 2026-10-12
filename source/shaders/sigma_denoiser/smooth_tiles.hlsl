#define g_ThreadCountX 16
#define g_ThreadCountY 16

#include "joint/sigma_denoiser_resources.hpp"
#include "sigma_denoiser/group_shared_preloader.hlsli"

BenzinDeclareRenderPassConsts(joint::SigmaConsts, g_PassConsts);
BenzinDeclareRootResource(Texture2D<float4>, g_Tiles, joint::SigmaSmoothTilesResources::Tiles);
BenzinDeclareRootResource(RWTexture2D<float2>, g_OutSmoothTiles, joint::SigmaSmoothTilesResources::OutSmoothTiles);

groupshared float g_IsPenumbra[g_SharedBufferSizeY][g_SharedBufferSizeX];

void Preload(uint2 localPos, uint2 globalPos)
{
    g_IsPenumbra[localPos.y][localPos.x] = g_Tiles[globalPos].x;
}

[numthreads(g_ThreadCountX, g_ThreadCountY, 1)]
void CsMain(sigma::GroupSharedCsInput input)
{
    SigmaPreloadToGroupSharedMem(input, g_PassConsts.TileCount, Preload);
    GroupMemoryBarrierWithGroupSync();

    const float3 centerTile = g_Tiles[input.PixelPos].xyz;
    const float gaussianFalloff = 1.01 / (centerTile.y + 0.01);

    float2 smoothPenumbra = 0.0;

    [unroll]
    for (uint j = 0; j <= SIGMA_BORDER * 2; ++j)
    {
        [unroll]
        for (uint i = 0; i <= SIGMA_BORDER * 2; ++i)
        {
            const float distance = length(float2(i, j) - SIGMA_BORDER);
            const float weight = exp2(-gaussianFalloff * distance * distance);

            const uint2 sharedPos = input.ThreadPos + uint2(i, j);
            smoothPenumbra += float2(g_IsPenumbra[sharedPos.y][sharedPos.x], 1.0) * weight;
        }
    }

    smoothPenumbra.x /= smoothPenumbra.y;

    // TODO: Add SIGMA_DEBUG define
    if (!g_PassConsts.IsTileSmoothingEnabled)
    {
        smoothPenumbra.x = centerTile.x;
    }

    g_OutSmoothTiles[input.PixelPos] = float2(smoothPenumbra.x, centerTile.z);
}
