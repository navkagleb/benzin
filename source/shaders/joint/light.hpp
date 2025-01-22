#pragma once

#include "hlsl_to_cpp.hpp"

namespace joint
{

    enum class LightType : uint
    {
        Sun,
        Spherical,
    };

    struct Light
    {
        float3 Color;
        float Intensity;
        float3 WorldPosition; // For sun - direction
        float WorldRadius; // For sun - tan of angle radius
        float3 Attenuation;
        LightType Type;
    };

}
