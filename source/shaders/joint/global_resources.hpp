#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class ReadbackStat
    {
        Geometry_TotalMeshletCount,
        Geometry_TotalTriangleCount,
        Geometry_RenderedMeshletCount,
        Geometry_RenderedTriangleCount,

        ProceduralGrass_PatchCount,
        ProceduralGrass_BladeCount,
        ProceduralGrass_VertexCount,
        ProceduralGrass_TriangleCount,
    };

    enum class FrustumPlane
    {
        Left,
        Right,
        Bottom,
        Top,
        Near,
        Far,
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

        float4 m_ViewFrustumPlanes[6];
    };

    struct FrameConsts
    {
        float2 RenderResolution;
        float2 InvRenderResolution;
        float MinRenderDimension;

        uint CpuFrameIndex;
        uint IsRenderResolutionChanged : 1;
        uint m_IsFrustumCullingEnabled : 1; // TODO: actually used only by geometry pass
        uint IsDenoiserEnabled : 1;

        float DeltaTimeInSec;
        float AnimationElapsedTimeInSec;
        float PrevAnimationElapsedTimeInSec;
        float2 m_Padding0;

        CameraConsts m_Camera;
        CameraConsts m_PrevCamera;
    };

}
