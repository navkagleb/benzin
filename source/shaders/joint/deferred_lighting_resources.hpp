#pragma once

namespace joint
{

    enum class Rc_DeferredLighting : uint32_t
    {
        AlbedoAndRoughness,
        EmissiveAndMetallic,
        WorldNormal,
        DepthStencil,
        Shadow,
    };

}
