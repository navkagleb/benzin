#pragma once

#include <shaders/joint/enum_types.hpp>

namespace sandbox
{

    struct RayTracingShadowsSettings
    {
        bool IsEnabled = true;
        uint32_t RaysPerPixel = 1;
    };

    struct SigmaDenoiserSettings
    {
        bool IsEnabled = true;

        float StabilizationStrength = 1.0f;

        bool IsPostBlurEnabled = true;
        bool IsTemporalStabilizationEnabled = true;
        bool IsBicubicSamplingUsedForHistory = true;
    };

    struct DeferredLightingSettings
    {
        float SunIntensity = 4.0f;
        DirectX::XMFLOAT3 SunColor{ 1.0f, 1.0f, 0.9f };

        float SunAngularDiameterInRadians = DirectX::XMConvertToRadians(0.5f); // [0.01f, 5.0f]
        float SunAzimuthInRadians = DirectX::XMConvertToRadians(0.0f); // [-180.0f, 180.0f]
        float SunElevationInRadians = DirectX::XMConvertToRadians(45.0f); // [0.0f, 180.0]
    };

    struct FullScreenDebugSettings
    {
        joint::DebugOutputType DebugOutputType = joint::DebugOutputType_None;
        uint32_t ViewDepthMipIndex = 0;
        float MinViewDepth = 0.0f;
        float MaxViewDepth = 20.0f;
    };

    DirectX::XMFLOAT3 GetSunDirection(const DeferredLightingSettings& settings);

}
