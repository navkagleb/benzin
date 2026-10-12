#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class EnvironmentResources : uint
    {
        CubeMap,
    };

    enum class EquirectangularToCubeResources : uint
    {
        EquirectangularTexture,
        OutCubeMap,
    };

}
