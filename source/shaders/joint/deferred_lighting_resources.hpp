#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class DeferredLightingResources : uint
    {
        AlbedoAndRoughness,
        EmissiveAndMetallic,
        WorldNormal,
        DepthStencil,
        Shadow,
    };

}
