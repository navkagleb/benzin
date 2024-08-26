#pragma once

#ifdef SIGMA_USE_BORDER_2
    #define SIGMA_BORDER 2
#else
    #define SIGMA_BORDER 1
#endif

namespace sigma
{

    static const float g_Fp16Max = 65504.0;
    static const float g_Eps = 1e-6;

    // Local light source
    // X => IN_PENUMBRA
    // "lightSize" must be an acceptable projection to the plane perpendicular to the light direction
    float PackPenumbra(float distanceToOccluder, float distanceToLight, float lightSize)
    {
        const float penumbraSize = lightSize * distanceToOccluder / max(distanceToLight - distanceToOccluder, g_Eps);
        const float penumbraRadius = penumbraSize * 0.5;

        return distanceToOccluder >= g_Fp16Max ? g_Fp16Max : min(penumbraRadius, 32768.0);
    }

}