#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct CameraConsts
    {
        float4x4 WorldToView;
        float4x4 ViewToWorld;

        float4x4 ViewToClip;
        float4x4 ClipToView;

        float4x4 WorldToClip;
        float4x4 ClipToWorld;
        float4x4 ClipToWorldNoTranslation;

        float3 WorldPosition;
        float PixelToWorldScale;

        float2 UvToViewScale;
        float2 UvToViewBias;
    };

    struct FrameConsts
    {
        float2 RenderResolution;
        float2 InvRenderResolution;
        float MinRenderDimension;

        uint CpuFrameIndex;
        uint LightCount;

        uint IsRenderResolutionChanged : 1;
        uint IsShadowsEnabled : 1;
        uint IsDenoiserEnabled : 1;

        float DeltaTimeInSec;

        BenzinAlign16 CameraConsts Camera;
        CameraConsts PrevCamera;
    };

}
