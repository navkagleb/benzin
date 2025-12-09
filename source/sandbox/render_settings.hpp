#pragma once

#include <benzin/graphics2/imgui_pass.hpp>
#include <shaders/joint/tone_mapping_resources.hpp>

namespace sandbox
{

    struct GBufferSettings
    {
        static const DXGI_FORMAT ms_Color0DxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Albedo, Albedo, Albedo, Roughness
        static const DXGI_FORMAT ms_Color1DxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Emissive, Emissive, Emissive, Metallic
        static const DXGI_FORMAT ms_Color2DxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; // WorldNormal, WorldNormal, WorldNormal, None
        static const DXGI_FORMAT ms_Color3DxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; // UvMv, UvMv, ViewDepthMv, None
        static const DXGI_FORMAT ms_Color4DxgiFormat = DXGI_FORMAT_R32_FLOAT; // ViewDepth
        static const DXGI_FORMAT ms_DepthStencilDxgiFormat = DXGI_FORMAT_D32_FLOAT;

        bool m_IsMeshPipelineEnabled = true;
        bool m_IsLodSelectionEnabled = true;
        bool m_IsFrustumCullingEnabled = true;
    };

    struct GBufferStats
    {
        D3D12_QUERY_DATA_PIPELINE_STATISTICS1 m_D3D12PipelineStats = {};
    };

    struct ProceduralGrassSettings
    {
        bool IsEnabled = false;
        bool IsFrustumCullingEnabled = true;

        float GrassPatchCullRadius = 0.1f;
        float GrassEndDistance = 20.0f;
        float SpacingInGrassPatch = 0.04f;
        float WindDirection = DirectX::XM_PI;
        float BladeWidth = 0.01f;
        DirectX::XMFLOAT3 BaseColor = { 189.0f / 256.0f, 236.0f / 256.0f, 76.0f / 256.0f };
    };

    struct ProceduralGrassStats
    {
        uint32_t MaxPatchCount = 0;

        uint32_t PatchCount = 0;
        uint32_t BladeCount = 0;
        uint32_t VertexCount = 0;
        uint32_t TriangleCount = 0;
    };

    struct RayTracingShadowSettings
    {
        bool m_IsAllowed = true;
        bool m_IsEnabled = true;

        bool m_IsBlueNoiseUsed = true;
        bool m_IsNoiseAnimated = true;
        bool m_IsBlueNoiseDepthFreezed = false;

        uint32_t m_BlueNoiseDepth = 0;
        uint32_t m_BlueNoiseDepthIndex = 0;
    };

    struct SigmaDenoiserSettings
    {
        static const DXGI_FORMAT ms_PenumbraDxgiFormat = DXGI_FORMAT_R16_FLOAT;
        static const uint32_t ms_MaxHistoryLength = 7;

        bool m_IsEnabled = true;
        float m_PlaneDistanceSensitivity = 0.02f; // (normalized %) - represents maximum allowed deviation from the local tangent plane
        float m_DisocclusionThreshold = 0.02f; // (normalized %)
        bool m_IsClearEnabled = false;
        bool m_IsTileSmoothingEnabled = true;
        bool m_IsPostBlurEnabled = true;
        bool m_IsTemporalStabilizationEnabled = true;

        uint32_t m_HistoryLength = 5;
        float m_StabilizationStrength = 0.0;
    };

    struct DeferredLightingSettings
    {
        static const DXGI_FORMAT ms_HdrColorDxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    };

    struct ToneMappingSettings
    {
        struct LuminanceHistogram
        {
            float m_MinLogLuminance = -12.0f;
            float m_MaxLogLuminance = 2.0;
            float m_Tau = 1.1f;
        };

        bool m_IsToneMappingEnabled = true;
        bool m_IsAutoExposureUsed = true;
        bool m_IsAccurateGammaCorrectionUsed = true;

        LuminanceHistogram m_LuminanceHistogram;

        joint::PbrCameraConsts m_PbrCamera
        {
            .m_Aperture = 8.0,
            .m_ShutterSpeed = 1.0f / 125.0f,
            .m_Iso = 100.0f,
        };

        joint::ToneReproductionTransform m_ToneReproductionTransform = joint::ToneReproductionTransform::AcesFilm;
    };

    template <typename SettingsT>
    class SettingsTool : public benzin::ImGuiTool
    {
    public:
        SettingsTool(std::string_view path, SettingsT& settings)
            : benzin::ImGuiTool{ path }
            , m_Settings{ settings }
        {}

        void DrawWindowContent() override
        {
            DrawSettings(m_Settings);
        }

    private:
        SettingsT& m_Settings;
    };

    template <typename SettingsT>
    void DrawSettings(SettingsT& settings);

}
