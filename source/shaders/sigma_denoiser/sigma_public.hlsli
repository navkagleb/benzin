#pragma once

namespace sigma
{

    static const float g_Fp16Max = 65504.0;
    static const float g_Eps = 1e-6;

    // Infinite ( directional ) light source
    // X => IN_PENUMBRA
    float PackPenumbra(float distanceToOccluder,float tanOfLightAngularRadius)
    {
        // Return the size of penumbra in world space

        const float penumbraSize = distanceToOccluder * tanOfLightAngularRadius;
        const float penumbraRadius = penumbraSize * 0.5;

        return distanceToOccluder >= g_Fp16Max ? g_Fp16Max : min(penumbraRadius, 32768.0);
    }
    
    // Local light source
    // X => IN_PENUMBRA
    // "lightSize" must be an acceptable projection to the plane perpendicular to the light direction
    float PackPenumbra(float distanceToOccluder, float distanceToLight, float lightSize)
    {
        const float penumbraSize = lightSize * distanceToOccluder / max(distanceToLight - distanceToOccluder, g_Eps);
        const float penumbraRadius = penumbraSize * 0.5;

        return distanceToOccluder >= g_Fp16Max ? g_Fp16Max : min(penumbraRadius, 32768.0);
    }

    float PackShadow(float shadow)
    {
        return sqrt(saturate(shadow));
    }

    float UnpackShadow(float shadow)
    {
        return shadow * shadow;
    }

    bool IsLit(float penumbra)
    {
        return penumbra >= sigma::g_Fp16Max;
    }

}