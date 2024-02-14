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
    };

    struct RTShadowPassConstants
    {
        float DeltaTime;
        float ElapsedTime;
        float RandomFloat01;
        uint RaysPerPixel;
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
