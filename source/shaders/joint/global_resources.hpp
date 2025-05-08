#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class ReadbackStat
    {
        ProceduralGrass_PatchCount,
        ProceduralGrass_BladeCount,
        ProceduralGrass_VertexCount,
        ProceduralGrass_TriangleCount,
    };

    enum class FrustumPlane
    {
        Near = 0,
        Far,
        Right,
        Left,
        Top,
        Bottom,

        Count,
    };

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

        // NOTE: The frustum planes are directed outside the frustum
        float4 WorldFrustumPlanes[(uint)FrustumPlane::Count];
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
        float AnimationElapsedTimeInSec;
        float PrevAnimationElapsedTimeInSec;

        BenzinAlign16 CameraConsts Camera;
        CameraConsts PrevCamera;
    };

}
