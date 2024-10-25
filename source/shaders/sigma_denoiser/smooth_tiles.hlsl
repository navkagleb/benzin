#include "joint/sigma_denoiser_resources.hpp"

#define RenderPassConstantsType joint::SigmaConstants
#include "unified_root_parameters.hlsli"

#include "sigma_denoiser/sigma_common.hlsli"
#include "sigma_denoiser/lds_preloader.hlsli"

BenzinDeclareRootResource(Texture2D<float4>, g_TilesTex, joint::Rc_SigmaSmoothTiles::TilesTex);
BenzinDeclareRootResource(RWTexture2D<float2>, g_OutSmoothTilesTex, joint::Rc_SigmaSmoothTiles::OutSmoothTilesTex);

static const uint g_GroupSize = 16;
static const uint g_BufferSize = g_GroupSize + SIGMA_BORDER * 2;

groupshared float g_Tiles[g_BufferSize][g_BufferSize];

void Preload(uint2 localPos, uint2 globalPos)
{
    g_Tiles[localPos.y][localPos.x] = g_TilesTex[globalPos].x;
}

struct CsInput
{
    uint2 ThreadPos : SV_GroupThreadID;
    uint2 PixelPos : SV_DispatchThreadID;
    uint FlatThreadIndex : SV_GroupIndex;
};

[numthreads(g_GroupSize, g_GroupSize, 1)]
void CsMain(CsInput input)
{
    {
        sigma::LdsPreloadCreation creation;
        creation.ThreadPos = input.ThreadPos;
        creation.PixelPos = input.PixelPos;
        creation.FlatThreadIndex = input.FlatThreadIndex;
        creation.GroupSize = g_GroupSize;
        creation.BufferSize = g_BufferSize;
        creation.Dimension = g_PassConstants.TileCount;

        SigmaRunLdsPreloader(creation, Preload);
        
        GroupMemoryBarrierWithGroupSync();
    }

    const float3 centerTile = g_TilesTex[input.PixelPos].xyz;
    const float k = 1.01 / (centerTile.y + 0.01);

    float blurry = 0.0;
    float weightSum = 0.0;

    [unroll]
    for (uint j = 0; j <= SIGMA_BORDER * 2; ++j)
    {
        [unroll]
        for (uint i = 0; i <= SIGMA_BORDER * 2; ++i)
        {
            const float distance = length(float2(i, j) - SIGMA_BORDER); // TODO: what name actually need to be used ???
            const float weight = exp2(-k * distance * distance);

            blurry += g_Tiles[input.ThreadPos.y + j][input.ThreadPos.x + i] * weight;
            weightSum += weight;
        }
    }

    blurry /= weightSum;

    g_OutSmoothTilesTex[input.PixelPos] = float2(blurry, centerTile.z);
}
