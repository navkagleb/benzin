#pragma once

namespace benzin
{

    enum class BufferId : uint32_t
    {
        ToneMapping_LuminanceHistogram,
    };

    enum class TextureId : uint32_t
    {
        // GBuffer
        AlbedoAndRoughness,
        EmissiveAndMetallic,
        WorldNormal,
        Mv,
        ViewDepth,
        DepthStencil,

        // RayTraced Shadows
        NoisyPenumbra,

        // SigmaDenoiser
        Sigma_Tiles,
        Sigma_SmoothTiles,
        Sigma_BlurredPenumbra1,
        Sigma_BlurredPenumbra2,
        Sigma_BlurredShadowTemp1,
        Sigma_BlurredShadowTemp2,
        Shadow = Sigma_BlurredShadowTemp2 + 2,
        ShadowHistoryLength = Shadow + 2,

        // Deferred Lighting
        HdrColor,

        // Tone Mapping
        ToneMapping_AvgLuminance,
        ToneMapping_DebugLuminanceHistogram,
        Final,

        // Engine Textures
        DebugTexture,
    };

    enum class PsoId : uint32_t
    {
        GeometryPass_DepthClockwise,
        GeometryPass_DepthCounterClockwise,
        GeometryPass_Clockwise,
        GeometryPass_CounterClockwise,

        ShadowPass,
        SigmaClassifyTiles,
        SigmaSmoothTiles,
        SigmaBlur,
        SigmaPostBlur,
        SigmaTemporalStabilization,
        DeferredLighting,
        Environment,
        Environment_EquirectangularToCube,

        ToneMapping_CalcLuminanceHistogram,
        ToneMapping_CalcAvgLuminance,
        ToneMapping_ApplyToneMapOperator,

        // Engine PSOs
        TextureViewer,
        ImGui,
    };

}

BenzinEnableUnaryPlusForEnum(benzin::BufferId);
BenzinEnableUnaryPlusForEnum(benzin::TextureId);
BenzinEnableUnaryPlusForEnum(benzin::PsoId);
