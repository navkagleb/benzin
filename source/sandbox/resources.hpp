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
        SigmaPrevShadow,
        SigmaPrevHistoryLength,
        SigmaPenumbra1,
        SigmaPenumbra2,
        SigmaShadowTemp1,
        SigmaShadowTemp2,
        SigmaShadow,
        SigmaHistoryLength,

        Final,
        ImGui,

        Count,
    };
    BenzinEnableUnaryPlusForEnum(Texture);

}
