#pragma once

#include "hlsl_to_cpp.hpp"

#include "enum_types.hpp"

namespace joint
{

    struct CameraConstants
    {
        float4x4 WorldToView;
        float4x4 WorldToViewForNormals;
        float4x4 InvWorldToView;

        float4x4 ViewToClip;
        float4x4 InvViewToClip;

        float4x4 WorldToClip;
        float4x4 InvWorldToClip;

        float4x4 InvDirectionWorldToClip;

        float3 WorldPosition;

        float4 PackedFrustumPlaneSlopes;
    };

    struct FrameConstants
    {
        float2 RenderResolution;
        float2 InvRenderResolution;
        float RenderAspectRatio;
        float PixelToWorldScale;

        uint IsShadowsEnabled : 1;
        uint IsDenoiserEnabled : 1;

        uint CpuFrameIndex;

        float4 RandomFloats01;

        CameraConstants Camera;
        CameraConstants PrevCamera;
    };

    struct RayTracingShadowsConstants
    {
        uint RaysPerPixel;
        float TanSunAngularRadius;
        float PixelAngularRadiusInRadians;
        float SunAngularRadiusInRadians;
        float3 SunDirection;
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
        MipGenerationFilterType FilterType;
    };

} // namespace joint
