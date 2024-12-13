#include "sigma_denoiser/sigma_common.hlsli"

#if !defined(g_ThreadCountX) || !(g_ThreadCountY)
    #error Please define g_ThreadCountX and g_ThreadCountY to use LDS preloader
#endif

#define g_SharedBufferSizeX (g_ThreadCountX + 2 * SIGMA_BORDER)
#define g_SharedBufferSizeY (g_ThreadCountY + 2 * SIGMA_BORDER)

namespace sigma
{

    struct GroupSharedCsInput
    {
        uint2 ThreadPos : SV_GroupThreadID;
        uint2 PixelPos : SV_DispatchThreadID;
        uint FlatThreadIndex : SV_GroupIndex;
    };

}

// const int2 groupBasePos = input.PixelPos - input.ThreadPos - SIGMA_BORDER = Is this equal to SV_GroupID?

#define SigmaPreloadToGroupSharedMem( \
    /* GroupSharedCsInput */ input, \
    /* uint2 */ resolution, \
    /* void(*)(uint2 sharedPos, uint2 globalPos) */ preloader \
    ) \
    { \
        const int2 groupBasePos = input.PixelPos - input.ThreadPos - SIGMA_BORDER; \
        const uint stageCount = DivideUp(g_SharedBufferSizeX * g_SharedBufferSizeY, g_ThreadCountX * g_ThreadCountY); \
        \
        [unroll] \
        for (uint stageIndex = 0; stageIndex < stageCount; ++stageIndex) \
        { \
            const uint flatTileIndex = input.FlatThreadIndex + stageIndex * g_ThreadCountX * g_ThreadCountY; \
            const uint2 localTilePos = uint2(flatTileIndex % g_SharedBufferSizeX, flatTileIndex / g_SharedBufferSizeX); \
            \
            if (stageIndex == 0 || flatTileIndex < g_SharedBufferSizeX * g_SharedBufferSizeY) \
            { \
                const uint2 globalTilePos = clamp(groupBasePos + localTilePos, 0, resolution - 1); \
                preloader(localTilePos, globalTilePos); \
            } \
        } \
    }
