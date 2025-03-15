#pragma once

#include <shaders/joint/full_screen_debug_resources.hpp>
#include <shaders/joint/tone_mapping_resources.hpp>

namespace sandbox
{

    struct GBufferStats
    {
        uint32_t MeshCount = 0;
        uint32_t RenderedMeshCount = 0;
        uint32_t RenderedTriangleCount = 0;
    };

    struct GBufferSettings
    {
        bool IsFrustumCullingEnabled = true;

        GBufferStats Stats;
    };

    struct RayTracing_ShadowSettings
    {
        bool IsEnabled = true;
        bool IsBlueNoiseUsed = true;
        bool IsNoiseAnimated = true;
    };

    struct SigmaDenoiserSettings
    {
        const benzin::GraphicsFormat PenumbraFormat = benzin::GraphicsFormat::R16Float;
        const uint32_t MaxHistoryLength = 7;

        bool IsEnabled = true;
        float PlaneDistanceSensitivity = 0.02f; // (normalized %) - represents maximum allowed deviation from the local tangent plane
        float DisocclusionThreshold = 0.02f; // (normalized %)
        bool IsClearEnabled = false;
        bool IsTileSmoothingEnabled = true;
        bool IsPostBlurEnabled = true;
        bool IsTemporalStabilizationEnabled = true;

        uint32_t HistoryLength = 5;
        float StabilizationStrength = 0.0;
    };

    struct ToneMappingSettings
    {
        struct LuminanceHistogram
        {
            float MinLogLuminance = -12.0f;
            float MaxLogLuminance = 2.0;
            float Tau = 1.1f;
        };

        bool IsToneMappingEnabled = true;
        bool IsAutoExposureUsed = true;
        bool IsAccurateGammaCorrectionUsed = true;

        LuminanceHistogram LuminanceHistogram;

        joint::PbrCameraConsts PbrCamera
        {
            .Aperture = 8.0,
            .ShutterSpeed = 1.0f / 125.0f,
            .Iso = 100.0f,
        };

        joint::ToneReproductionTransform ToneReproductionTransform = joint::ToneReproductionTransform::AcesFilm;
    };

    struct FullScreenDebugSettings
    {
        joint::DebugOutputType DebugOutputType = joint::DebugOutputType::None;
        uint32_t ViewDepthMipIndex = 0;
        float MinViewDepth = 0.0f;
        float MaxViewDepth = 20.0f;
    };

}
