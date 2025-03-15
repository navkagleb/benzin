#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct ImGuiConsts
    {
        float4x4 ViewToClipOrtho;
    };

    enum class ImGuiSamplerIndex : uint
    {
        Point,
        Linear,
    };

    enum class ImGuiResources : uint
    {
        Texture,
        SamplerIndex,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType0)
    #define BenzinRenderPassConstsType0 joint::ImGuiConsts
#endif
