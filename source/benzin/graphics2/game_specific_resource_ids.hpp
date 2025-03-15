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
    };

    enum class PsoId : uint32_t
    {
        GeometryPass,
        ShadowPass,
        SigmaClassifyTiles,
        SigmaSmoothTiles,
        SigmaBlur,
        SigmaPostBlur,
        SigmaTemporalStabilization,
        DeferredLighting,
        Environment,
        Environment_EquirectangularToCube,
        FullScreenDebug,

        ToneMapping_CalcLuminanceHistogram,
        ToneMapping_CalcAvgLuminance,
        ToneMapping_ApplyToneMapOperator,

        ImGui,
    };

    inline constexpr auto g_InvalidBufferId = BufferId{ g_InvalidUnsigned<uint32_t> };
    inline constexpr auto g_InvalidTextureId = TextureId{ g_InvalidUnsigned<uint32_t> };
    inline constexpr auto g_InvalidPsoId = PsoId{ g_InvalidUnsigned<uint32_t> };

}

BenzinEnableUnaryPlusForEnum(benzin::BufferId);
BenzinEnableUnaryPlusForEnum(benzin::TextureId);
BenzinEnableUnaryPlusForEnum(benzin::PsoId);
