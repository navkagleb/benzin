#pragma once

#include <shaders/joint/enum_types.hpp>

namespace sandbox
{

    struct RtShadowsSettings
    {
        bool IsRtShadowEnabled = true;
        uint32_t RaysPerPixel = 1;
    };

    struct DenoiserSettings
    {
        bool IsDenoiserEnabled = true;
        uint32_t MaxTemporalAccumulationCount = 16;
    };

    struct DenoiserMipGenerationSettings
    {
        joint::MipGenerationFilterType DepthFilterType = joint::MipGenerationFilterType_Min;
    };

    struct DenoiserHistoryFixSettings
    {
        bool IsHistoryFixEnabled = true;
        bool IsViewDepthUsedForWeights = true;
    };

    struct DenoiserBlurSettings
    {
        float SpecularAccumulationCurve = 0.2f;
        float SpecularAccumulationBasePower = 0.25f;

        bool IsDenoiserAntilagEnabled = true;

        bool IsGeometryWeightUsed = true;
        bool IsNormalWeightUsed = true;
        bool IsRoughnessWeightUsed = true;
        float GeometryWeightSensitivity = 20.0f;
        float MinBlurRadius = 0.01f;
        float MaxBlurRadius = 0.2f;
    };

    struct DeferredLightingSettings
    {
        float SunIntensity = 0.0f;
        DirectX::XMFLOAT3 SunColor{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 SunDirection{ -0.5f, -0.5f, -0.5f };
    };

    struct FullScreenDebugSettings
    {
        joint::DebugOutputType DebugOutputType = joint::DebugOutputType_None;
        uint32_t ViewDepthMipIndex = 0;
        float MinViewDepth = 0.0f;
        float MaxViewDepth = 20.0f;
    };

}
