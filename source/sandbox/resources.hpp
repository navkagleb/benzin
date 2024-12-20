#pragma once

namespace sandbox
{

    enum class Texture : uint32_t
    {
        // GBuffer
        AlbedoAndRoughness,
        EmissiveAndMetallic,
        WorldNormal,
        VelocityBuffer,
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

        Final,
        ImGui,

        Count,
    };
    BenzinEnableUnaryPlusForEnum(Texture);

}
