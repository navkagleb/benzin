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

        uint CpuFrameIndex;
        float FrameTimeInSec;
        float ElapsedTimeInSec;

        float4 RandomFloats01;

        CameraConstants Camera;
        CameraConstants PrevCamera;
    };

    struct RayTracingShadowsConstants
    {
        bool IsEnabled;
        uint RaysPerPixel;
        float TanSunAngularRadius;
        float PixelAngularRadiusInRadians;
        float SunAngularRadiusInRadians;
        float3 SunDirection;
    };

    struct DenoiserTemporalAccumulationConstants
    {
        bool IsAccumulationEnabled : 1;
    };

    struct DenoiserHistoryFixConstants
    {
        uint IsHistoryFixEnabled : 1;
        uint IsViewDepthUsedForWeights : 1;
    };

    struct DenoiserBlurConstants
    {
        float SpecularAccumulationCurve;
        float SpecularAccumulationBasePower;

        uint IsDenoiserAntilagEnabled : 1;

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
        MipGenerationFilterType FilterType;
    };

} // namespace joint
