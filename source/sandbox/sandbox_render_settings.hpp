#pragma once

#include <shaders/joint/enum_types.hpp>

namespace sandbox
{

    struct GBufferStats
    {
        uint32_t MeshCount = 0;
        uint32_t RenderedMeshCount = 0;
    };

    struct RayTracingShadowsSettings
    {
        bool IsEnabled = true;
        bool IsBlueNoiseUsed = true;
        bool IsNoiseAnimated = false;
    };

    struct SigmaDenoiserSettings
    {
        bool IsEnabled = false;
        float PlaneDistanceSensitivity = 0.02f; // (normalized %) - represents maximum allowed deviation from the local tangent plane
        float DisocclusionThreshold = 0.02f; // (normalized %)
        bool IsClearEnabled = false;
        bool IsTileSmoothingEnabled = true;
        bool IsPostBlurEnabled = true;
        bool IsTemporalStabilizationEnabled = true;

        uint32_t MaxHistoryLength = 5;
        float StabilizationStrength = 0.0;
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
