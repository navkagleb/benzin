#pragma once

namespace benzin
{

    enum class TextureId : uint32_t
    {
        AlbedoAndRoughness,
        EmissiveAndMetallic,
        WorldNormal,
        Mv,
        ViewDepth,
        Depth,
        Hzb,

        NoisyPenumbra,

        Sigma_Tiles,
        Sigma_SmoothTiles,
        Sigma_BlurredPenumbra1,
        Sigma_BlurredPenumbra2,
        Sigma_BlurredTempShadow1,
        Sigma_BlurredTempShadow2,

        Shadow = Sigma_BlurredTempShadow2 + 2,
        ShadowHistoryLength = Shadow + 2,

        HdrColor,
        Final,

        DebugTexture, // TOOD: Should be convered by another allocator (not resolution dependent)
    };

    enum class PsoId : uint32_t
    {
        Geometry_EarlyComputeCulling,
        Geometry_LateComputeCulling,
        Geometry_Vertex,
        Geometry_Mesh,
        Geometry_HzbGeneration,

        ProceduralGrass,

        ShadowPass,
        Sigma_ClassifyTiles,
        Sigma_SmoothTiles,
        Sigma_Blur,
        Sigma_PostBlur,
        Sigma_TemporalStabilization,
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

BenzinAllowDereferenceOperatorForEnum(benzin::TextureId);
BenzinAllowDereferenceOperatorForEnum(benzin::PsoId);
