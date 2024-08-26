#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum SigmaClassifyTilesRs : uint32_t
    {
        SigmaClassifyTilesRc_ViewDepthTex,
        SigmaClassifyTilesRc_PenumbraTex,

        SigmaClassifyTilesRc_OutTilesTex,
    };

    enum SigmaSmoothTilesRc : uint32_t
    {
        SigmaSmoothTilesRc_TilesTex,

        SigmaSmoothTilesRc_OutSmoothTilesTex,
    };

    enum SigmaBlurRc : uint32_t
    {
        SigmaBlurRc_DepthTex, // TODO: ViewPos can be reconstructed from ViewDepthTex

        SigmaBlurRc_WorldNormalTex,
        SigmaBlurRc_ViewDepthTex,
        SigmaBlurRc_AlbedoAndRoughnessTex,
        SigmaBlurRc_PenumbraTex,
        SigmaBlurRc_SmoothTilesTex,
        SigmaBlurRc_HistoryTex,

        SigmaBlurRc_OutDenoisedPenumbraTex,
        SigmaBlurRc_OutHistoryTex,
    };

    struct SigmaConstants
    {
        uint2 TileCount;
        float StabilizationStrength;
    };

}
