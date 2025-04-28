#pragma once

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
        static constexpr auto s_Color0Format = benzin::GraphicsFormat::Rgba8Unorm; // Albedo, Albedo, Albedo, Roughness
        static constexpr auto s_Color1Format = benzin::GraphicsFormat::Rgba8Unorm; // Emissive, Emissive, Emissive, Metallic
        static constexpr auto s_Color2Format = benzin::GraphicsFormat::Rgba16Float; // WorldNormal, WorldNormal, WorldNormal, None
        static constexpr auto s_Color3Format = benzin::GraphicsFormat::Rgba16Float; // UvMv, UvMv, ViewDepthMv, None
        static constexpr auto s_Color4Format = benzin::GraphicsFormat::R32Float; // ViewDepth

        static constexpr auto s_DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;

        bool IsDepthPrePassEnabled = true;
        bool IsFrustumCullingEnabled = true;
        bool IsMeshShaderUsed = true;
    };

    struct ProceduralGrassStats
    {
        uint32_t MaxPatchCount = 0;

        uint32_t PatchCount = 0;
        uint32_t BladeCount = 0;
        uint32_t VertexCount = 0;
        uint32_t TriangleCount = 0;
    };

    struct ProceduralGrassSettings
    {
        bool IsEnabled = true;
        bool IsFrustumCullingEnabled = true;

        float GrassPatchCullRadius = 0.1f;
        float GrassEndDistance = 20.0f;
        float SpacingInGrassPatch = 0.04f;
        float WindDirection = DirectX::XM_PI;
        float BladeWidth = 0.01f;
        DirectX::XMFLOAT3 BaseColor{ 189.0f / 256.0f, 236.0f / 256.0f, 76.0f / 256.0f };
    };

    struct RayTracing_ShadowSettings
    {
        bool IsEnabled = true;

        bool IsBlueNoiseUsed = true;
        bool IsNoiseAnimated = true;
        bool IsBlueNoiseDepthFreezed = false;

        uint16_t BlueNoiseDepth = 0;
        uint16_t BlueNoiseDepthIndex = 0;
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

    struct DeferredLightingSettings
    {
        static constexpr auto s_HdrColorFormat = benzin::GraphicsFormat::Rgba16Float;
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

}
