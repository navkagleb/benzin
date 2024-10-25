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
        SigmaTiles,
        SigmaSmoothTiles,
        SigmaHistory,
        SigmaDenoisedPenumbra,
        SigmaShadowTemp1,
        SigmaShadowTemp2,
        SigmaShadow,

        Final,
        ImGui,

        Count,
    };
    BenzinEnableUnaryPlusForEnum(Texture);

}
