#pragma once

#include "sigma_denoiser/sigma_common.hlsli"

#define SigmaCalcSharedBufferSize(threadCount) (threadCount + 2 * SIGMA_BORDER)

namespace sigma
{

    struct LdsPreloadCreation
    {
        uint2 ThreadPos;
        uint2 PixelPos;
        uint FlatThreadIndex;

        uint2 GroupSize;
        uint2 BufferSize;

        uint2 Dimension;
    };

}

// const int2 groupBasePos = creation.PixelPos - creation.ThreadPos - SIGMA_BORDER = Is this equal to SV_GroupID?

#define SigmaRunLdsPreloader( \
    /* LdsPreloadCreation*/ creation, \
    /* void(*)(uint2 sharedPos, uint2 globalPos) */ preloader \
    ) \
    { \
        const int2 groupBasePos = creation.PixelPos - creation.ThreadPos - SIGMA_BORDER; \
        const uint stageCount = sigma::DivideUp(creation.BufferSize.x * creation.BufferSize.y, creation.GroupSize.x * creation.GroupSize.y); \
        \
        [unroll] \
        for (uint stageIndex = 0; stageIndex < stageCount; ++stageIndex) \
        { \
            const uint flatTileIndex = creation.FlatThreadIndex + stageIndex * creation.GroupSize.x * creation.GroupSize.y; \
            const uint2 localTilePos = uint2(flatTileIndex % creation.BufferSize.x, flatTileIndex / creation.BufferSize.x); \
            \
            if (stageIndex == 0 || flatTileIndex < creation.BufferSize.x * creation.BufferSize.y) \
            { \
                const uint2 globalTilePos = clamp(groupBasePos + localTilePos, 0, creation.Dimension - 1); \
                preloader(localTilePos, globalTilePos); \
            } \
        } \
    }
