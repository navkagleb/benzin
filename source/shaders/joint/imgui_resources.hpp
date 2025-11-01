#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct ImGuiConsts
    {
        float4x4 m_ViewToClipOrtho;
    };

    enum class ImGuiSamplerIndex : uint
    {
        Point = 1, // NOTE: To reserve 0 as an invalid value for ImDrawCmd::TextureId, so that ImGuiPass::PackImTextureId always returns at least 1.
        Linear,
    };

    enum class ImGuiResources : uint
    {
        Texture,
        SamplerIndex,
    };

}

#if !defined(__cplusplus) && !defined(BenzinRenderPassConstsType)
    #define BenzinRenderPassConstsType joint::ImGuiConsts
#endif
