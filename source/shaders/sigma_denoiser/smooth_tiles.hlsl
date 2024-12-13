#define g_ThreadCountX 16
#define g_ThreadCountY 16

#include "joint/sigma_denoiser_resources.hpp"

#define RenderPassConstantsType joint::SigmaConstants
#include "unified_root_parameters.hlsli"

#include "sigma_denoiser/group_shared_preloader.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_Tiles, joint::Rc_SigmaSmoothTiles::Tiles);
BenzinDeclareRootResource(RWTexture2D<float2>, g_OutSmoothTiles, joint::Rc_SigmaSmoothTiles::OutSmoothTiles);

groupshared float g_IsPenumbra[g_SharedBufferSizeY][g_SharedBufferSizeX];

void Preload(uint2 localPos, uint2 globalPos)
{
    g_IsPenumbra[localPos.y][localPos.x] = g_Tiles[globalPos].x;
}

[numthreads(g_ThreadCountX, g_ThreadCountY, 1)]
void CsMain(sigma::GroupSharedCsInput input)
{
    SigmaPreloadToGroupSharedMem(input, g_PassConstants.TileCount, Preload);
    GroupMemoryBarrierWithGroupSync();

    const float3 centerTile = g_Tiles[input.PixelPos].xyz;
    const float k = 1.01 / (centerTile.y + 0.01);

    float2 smoothPenumbra = 0.0;

    [unroll]
    for (uint j = 0; j <= SIGMA_BORDER * 2; ++j)
    {
        [unroll]
        for (uint i = 0; i <= SIGMA_BORDER * 2; ++i)
        {
            const float distance = length(float2(i, j) - SIGMA_BORDER); // TODO: what name actually need to be used ???
            const float weight = exp2(-k * distance * distance);

            smoothPenumbra += float2(g_IsPenumbra[input.ThreadPos.y + j][input.ThreadPos.x + i], 1.0) * weight;
        }
    }

    smoothPenumbra.x /= smoothPenumbra.y;

    g_OutSmoothTiles[input.PixelPos] = float2(smoothPenumbra.x, centerTile.z);
}
