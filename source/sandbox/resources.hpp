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
        DepthStencil,
        ViewDepth,

        // RtShadows
        NoisyPenumbra,

        // SigmaDenoiser
        // TODO: Make 'Sigma' textures private to SigmaDenoiserPass
        SigmaTiles,
        SigmaSmoothTiles,
        SigmaPenumbra1,
        SigmaPenumbra2,
        SigmaShadowTemp1,
        SigmaShadowTemp2,
        SigmaShadow = SigmaShadowTemp2 + 2,
        SigmaHistoryLength = SigmaShadow + 2,

        Final,
        ImGui,

        Count,
    };
    BenzinEnableUnaryPlusForEnum(Texture);

}
