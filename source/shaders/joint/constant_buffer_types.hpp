#pragma once

#include "hlsl_to_cpp.hpp" // #TODO: Include directory must be started from 'shaders'

namespace joint
{

    struct CameraConstants
    {
        float4x4 View;
        float4x4 InverseView;

        float4x4 Projection;
        float4x4 InverseProjection;

        float4x4 ViewProjection;
        float4x4 InverseViewProjection;

        float4x4 InverseViewDirectionProjection;

        float3 WorldPosition;

        float __UnusedPadding;
    };

    struct DoubleFrameCameraConstants
    {
        CameraConstants CurrentFrame;
        CameraConstants PreviousFrame;
    };

    struct RTShadowPassConstants
    {
        float DeltaTime;
        uint CurrentTextureSlot;
        uint RaysPerPixel;
    };

    struct RTShadowDenoisingPassConstants
    {
        float2 TextureDimensions;
        uint CurrentTextureSlot;
        uint PreviousTextureSlot;
        bool IsForceCurrentVisiblity;
    };

    struct DeferredLightingPassConstants
    {
        float3 SunColor;
        float SunIntensity;
        float3 SunDirection;
        uint ActivePointLightCount;
        uint OutputType;
    };

} // namespace joint
