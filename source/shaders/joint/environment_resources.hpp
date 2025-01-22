#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class Rc_Environment : uint
    {
        CubeMap,
    };

    enum class Rc_EquirectangularToCube : uint
    {
        EquirectangularTexture,
        OutCubeMap,
    };

}
