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
        Left,
        Right,
        Bottom,
        Top,
        Near,
        Far,
    };

    struct CameraConsts
    {
        float4x4 m_WorldToView;
        float4x4 m_ViewToWorld;

        float4x4 m_ViewToClip;
        float4x4 m_ClipToView;

        float4x4 m_WorldToClip;
        float4x4 m_ClipToWorld;
        float4x4 m_ClipToWorldNoTranslation;

        float3 m_WorldPosition;
        float m_PixelToWorldScale;

        float2 m_UvToViewScale;
        float2 m_UvToViewBias;

        float4 m_ViewFrustumPlanes[6];
    };

    struct FrameConsts
    {
        float2 m_RenderResolution;
        float2 m_InvRenderResolution;
        float m_MinRenderDimension;

        uint m_CpuFrameIndex;
        uint m_IsRenderResolutionChanged : 1;
        uint m_IsFrustumCullingEnabled : 1; // TODO: actually used only by geometry pass
        uint m_IsLodSelectionEnabled : 1;
        uint m_IsDenoiserEnabled : 1;

        float m_DeltaTimeInSec;
        float m_AnimationElapsedTimeInSec;
        float m_PrevAnimationElapsedTimeInSec;
        float2 m_Padding0;

        CameraConsts m_Camera;
        CameraConsts m_PrevCamera;
    };

}
