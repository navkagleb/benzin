#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct TextureViewerConsts
    {
        uint4 ChannelMask;
        float MinColor;
        float MaxColor;
        uint2 TextureResolution;
    };

    enum class TextureViewerResources : uint
    {
        ReferenceTexture,
        OutDebugTexture,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::TextureViewerConsts
#endif
