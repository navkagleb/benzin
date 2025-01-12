#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    struct PointLight
    {
        float3 Color;
        float Intensity;
        float3 WorldPosition;
        float ConstantAttenuation;
        float LinearAttenuation;
        float ExponentialAttenuation;

        float GeometryRadius;
    };

}
