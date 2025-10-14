#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class ReadbackStat
    {
        Geometry_TotalMeshletCount,
        Geometry_TotalMeshletVertexCount,
        Geometry_TotalMeshletTriangleCount,

        Geometry_MeshletCount,
        Geometry_MeshletVertexCount,
        Geometry_MeshletTriangleCount,

        Geometry_VsInvocationCount,
        Geometry_AsInvocationCount,
        Geometry_MsInvocationCount,
        Geometry_PsInvocationCount,

        ProceduralGrass_PatchCount,
        ProceduralGrass_BladeCount,
        ProceduralGrass_VertexCount,
        ProceduralGrass_TriangleCount,
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

        float TanHalfFovX;
        float TanHalfFovY;
        float NearPlane;
        float FarPlane;

        float2 UvToViewScale;
        float2 UvToViewBias;
    };

    struct FrameConsts
    {
        float2 RenderResolution;
        float2 InvRenderResolution;
        float MinRenderDimension;

        uint CpuFrameIndex;
        uint LightCount; // TODO: Remove

        uint IsRenderResolutionChanged : 1;
        uint IsShadowsEnabled : 1;
        uint IsDenoiserEnabled : 1;

        float DeltaTimeInSec;
        float AnimationElapsedTimeInSec;
        float PrevAnimationElapsedTimeInSec;
        float m_Padding0;

        CameraConsts Camera;
        CameraConsts PrevCamera;
    };

}
