#pragma once

namespace joint
{

    enum class DepthResprojectionResources
    {
        PrevDepth,
        ReprojectedDepth,
    };

    struct DepthReductionPassConsts
    {
        float2 m_DestMipTexelSize;
        uint m_IsSourceWidthOdd;
        uint m_IsSourceHeightOdd;
    };

    enum class DepthReductionResources : uint
    {
        SourceMip,
        DestMip,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0) && defined(DEPTH_REDUCTION)
    #define BenzinRenderPassConstsType0 joint::DepthReductionPassConsts
#endif
