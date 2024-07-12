#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct CameraConstants
    {
        float4x4 View;
        float4x4 ViewForNormals;
        float4x4 InverseView;

        float4x4 Projection;
        float4x4 InverseProjection;

        float4x4 ViewProjection;
        float4x4 InverseViewProjection;

        float4x4 InverseViewDirectionProjection;

        float3 WorldPosition;

        float __UnusedPadding;
    };

    struct FrameConstants
    {
        float2 RenderResolution;
        float2 InvRenderResolution;
        uint CpuFrameIndex;
        float DeltaTime;
        float ElapsedTime;

        uint IsRtShadowsEnabled : 1;
        uint IsDenoiserEnabled : 1;
        uint MaxTemporalAccumulationCount;

        CameraConstants CurrentCamera;
        CameraConstants PreviousCamera;
    };

    struct RtShadowPassConstants
    {
        uint CurrentTextureSlot;
        uint RaysPerPixel;
    };

    struct DenoiserBlurConstants
    {
        uint IsGeometryWeightUsed : 1;
        uint IsNormalWeightUsed : 1;
        uint IsRoughnessWeightUsed : 1;
        float GeometryWeightSensitivity;
        float MinBlurRadius;
        float MaxBlurRadius;
    };

    struct DeferredLightingPassConstants
    {
        float3 SunColor;
        float SunIntensity;
        float3 SunDirection;
        uint ActivePointLightCount;
        uint OutputType;
    };

    struct FullScreenDebugConstants
    {
        uint OutputType;
        uint ViewDepthMipIndex;
        float MinViewDepth;
        float MaxViewDepth;
    };

    struct MipGenerationConstants
    {
        float2 InvDispatchDimensions;
        uint IsSourceWidthOdd;
        uint IsSourceHeightOdd;
        uint DestinationMipCount;
    };

} // namespace joint
