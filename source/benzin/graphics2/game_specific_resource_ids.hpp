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
        DepthStencil,

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
        GeometryPass_EarlyComputeCulling,
        GeometryPass_LateComputeCulling,
        GeometryPass_Vertex,
        GeometryPass_Mesh,

        ProceduralGrass,

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

BenzinAllowDereferenceOperatorForEnum(benzin::TextureId);
BenzinAllowDereferenceOperatorForEnum(benzin::PsoId);
